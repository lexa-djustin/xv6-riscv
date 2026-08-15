#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "ioe_trace.h"

struct ioe_trace_event ioe_trace_events[IOE_TRACE_CAPACITY];
volatile uint64 ioe_trace_write_sequence;
uint64 ioe_trace_capacity = IOE_TRACE_CAPACITY;
uint64 ioe_trace_event_size = sizeof(struct ioe_trace_event);
uint64 ioe_trace_timebase_hz = 10000000;
struct ioe_run_stats_snapshot ioe_run_stats;

static int ioe_trace_started;
static uint64 ioe_trace_instruction_baseline;
static uint64 ioe_trace_system_calls;
static uint64 ioe_trace_context_switches;
static uint64 ioe_trace_timer_interrupts;

static inline uint64
ioe_read_instret(void)
{
  uint64 value;
  asm volatile("csrr %0, instret" : "=r" (value));
  return value;
}


struct ioe_kernel_context_snapshot ioe_kernel_context_capture;

void
ioe_capture_kernel_context(struct proc *p)
{
  struct ioe_kernel_context_snapshot *capture = &ioe_kernel_context_capture;
  uint64 sequence = capture->sequence + 1;
  uint64 frame_pointer;
  asm volatile("mv %0, s0" : "=r" (frame_pointer));
  uint64 stack_pointer = r_sp();
  uint64 stack_top = PGROUNDUP(frame_pointer);
  uint64 frame_count = 0;


  asm volatile(
    "sd zero,   0(%0)\n"
    "sd ra,     8(%0)\n"
    "sd sp,    16(%0)\n"
    "sd gp,    24(%0)\n"
    "sd tp,    32(%0)\n"
    "sd t0,    40(%0)\n"
    "sd t1,    48(%0)\n"
    "sd t2,    56(%0)\n"
    "sd s0,    64(%0)\n"
    "sd s1,    72(%0)\n"
    "sd a0,    80(%0)\n"
    "sd a1,    88(%0)\n"
    "sd a2,    96(%0)\n"
    "sd a3,   104(%0)\n"
    "sd a4,   112(%0)\n"
    "sd a5,   120(%0)\n"
    "sd a6,   128(%0)\n"
    "sd a7,   136(%0)\n"
    "sd s2,   144(%0)\n"
    "sd s3,   152(%0)\n"
    "sd s4,   160(%0)\n"
    "sd s5,   168(%0)\n"
    "sd s6,   176(%0)\n"
    "sd s7,   184(%0)\n"
    "sd s8,   192(%0)\n"
    "sd s9,   200(%0)\n"
    "sd s10,  208(%0)\n"
    "sd s11,  216(%0)\n"
    "sd t3,   224(%0)\n"
    "sd t4,   232(%0)\n"
    "sd t5,   240(%0)\n"
    "sd t6,   248(%0)\n"
    :
    : "r" (capture->registers)
    : "memory"
  );

  capture->sequence = 0;
  capture->pid = p->pid;
  capture->resume_user_pc = p->trapframe->epc;
  capture->stack_pointer = stack_pointer;
  safestrcpy(
    capture->process_name,
    p->name,
    sizeof(capture->process_name)
  );

  while(frame_count < IOE_KERNEL_CONTEXT_MAX_FRAMES &&
        frame_pointer >= stack_top - PGSIZE + 16 &&
        frame_pointer < stack_top) {
    uint64 return_address = *(uint64 *)(frame_pointer - 8);
    uint64 next_frame_pointer = *(uint64 *)(frame_pointer - 16);
    if(return_address == 0)
      break;
    capture->frame_addresses[frame_count] = return_address;
    capture->frame_pointers[frame_count] = frame_pointer;
    frame_count++;
    if(next_frame_pointer <= frame_pointer || next_frame_pointer > stack_top)
      break;
    frame_pointer = next_frame_pointer;
  }

  capture->frame_count = frame_count;
  capture->program_counter =
    frame_count == 0 ? 0 : capture->frame_addresses[0];
  __sync_synchronize();
  capture->sequence = sequence;
}

static void
ioe_trace_record(uchar type, struct proc *p, uchar reason)
{
  int interrupts_enabled = intr_get();
  if(interrupts_enabled)
    intr_off();

  uint64 sequence = ioe_trace_write_sequence + 1;
  struct ioe_trace_event *event =
    &ioe_trace_events[(sequence - 1) % IOE_TRACE_CAPACITY];

  event->sequence = 0;
  event->time = r_time();
  event->pid = p == 0 ? 0 : p->pid;
  event->hart = cpuid();
  event->type = type;
  event->reason = reason;
  if(p == 0)
    event->process_name[0] = 0;
  else
    safestrcpy(event->process_name, p->name, sizeof(event->process_name));

  __sync_synchronize();
  event->sequence = sequence;
  __sync_synchronize();
  ioe_trace_write_sequence = sequence;

  if(interrupts_enabled)
    intr_on();
}

void
ioe_trace_process(uchar type, struct proc *p, uchar reason)
{
  if(type == IOE_TRACE_DISPATCH)
    ioe_trace_context_switches++;
  ioe_trace_record(type, p, reason);
}

void
ioe_trace_kernel_entry(struct proc *p, uint64 cause)
{
  uchar reason = (uchar)cause;
  if(cause == 8) {
    ioe_trace_system_calls++;
    uint64 syscall_number = p->trapframe->a7;
    reason = IOE_TRACE_SYSCALL_REASON_FLAG;
    if(syscall_number < IOE_TRACE_SYSCALL_REASON_FLAG)
      reason |= (uchar)syscall_number;
  }
  ioe_trace_record(IOE_TRACE_KERNEL_ENTER, p, reason);
}

void
ioe_trace_scheduler(void)
{
  ioe_trace_record(IOE_TRACE_SCHEDULER, 0, 0);
}

void
ioe_trace_idle(void)
{
  ioe_trace_record(IOE_TRACE_IDLE_ENTER, 0, 0);
}

void
ioe_trace_heartbeat(void)
{
  ioe_trace_record(IOE_TRACE_HEARTBEAT, 0, 0);
}

void
ioe_trace_timer_interrupt(void)
{
  ioe_trace_timer_interrupts++;
  uint64 retired = ioe_read_instret();
  if(!ioe_trace_started) {
    ioe_trace_instruction_baseline = retired;
    ioe_trace_started = 1;
  }
  ioe_run_stats.guest_instructions =
    retired - ioe_trace_instruction_baseline;
  ioe_run_stats.system_calls = ioe_trace_system_calls;
  ioe_run_stats.context_switches = ioe_trace_context_switches;
  ioe_run_stats.timer_interrupts = ioe_trace_timer_interrupts;
  ioe_trace_heartbeat();
}
