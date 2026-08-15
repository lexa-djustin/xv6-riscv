#ifndef IOE_TRACE_H
#define IOE_TRACE_H

#define IOE_TRACE_CAPACITY 4096
#define IOE_TRACE_PROCESS_NAME_LENGTH 16
#define IOE_TRACE_SYSCALL_REASON_FLAG 0x80

enum ioe_trace_type {
  IOE_TRACE_DISPATCH = 1,
  IOE_TRACE_SCHEDULER = 2,
  IOE_TRACE_USER_ENTER = 3,
  IOE_TRACE_KERNEL_ENTER = 4,
  IOE_TRACE_IDLE_ENTER = 5,
  IOE_TRACE_HEARTBEAT = 6,
  IOE_TRACE_PROCESS_REAP = 7,
  IOE_TRACE_SIGNAL_GENERATED = 8,
  IOE_TRACE_SIGNAL_DELIVERED = 9,
  IOE_TRACE_SIGNAL_IGNORED = 10,
  IOE_TRACE_SIGNAL_HANDLER = 11,
};

// Fixed-size little-endian records consumed by the Explorer through GDB.
struct ioe_trace_event {
  uint64 sequence;
  uint64 time;
  uint pid;
  ushort hart;
  uchar type;
  uchar reason;
  char process_name[IOE_TRACE_PROCESS_NAME_LENGTH];
};


#define IOE_KERNEL_CONTEXT_MAX_FRAMES 8
#define IOE_KERNEL_CONTEXT_REGISTER_COUNT 32

// The guest records the last kernel-to-user handoff without stopping QEMU.
// The Explorer reads this snapshot later, while GDB is already paused.
struct ioe_kernel_context_snapshot {
  volatile uint64 sequence;
  uint64 resume_user_pc;
  uint64 program_counter;
  uint64 stack_pointer;
  uint64 frame_count;
  uint pid;
  char process_name[IOE_TRACE_PROCESS_NAME_LENGTH];
  uint64 frame_addresses[IOE_KERNEL_CONTEXT_MAX_FRAMES];
  uint64 frame_pointers[IOE_KERNEL_CONTEXT_MAX_FRAMES];
  uint64 registers[IOE_KERNEL_CONTEXT_REGISTER_COUNT];
};

extern struct ioe_kernel_context_snapshot ioe_kernel_context_capture;
void ioe_capture_kernel_context(struct proc *);


// Low-frequency counters read while GDB has already paused QEMU for a
// timeline sample. Keeping these values outside each trace record preserves
// the compact event path used by syscall-heavy console programs.
struct ioe_run_stats_snapshot {
  uint64 guest_instructions;
  uint64 system_calls;
  uint64 context_switches;
  uint64 timer_interrupts;
};

extern struct ioe_run_stats_snapshot ioe_run_stats;

extern struct ioe_trace_event ioe_trace_events[IOE_TRACE_CAPACITY];
extern volatile uint64 ioe_trace_write_sequence;
extern uint64 ioe_trace_capacity;
extern uint64 ioe_trace_event_size;
extern uint64 ioe_trace_timebase_hz;

void ioe_trace_process(uchar, struct proc *, uchar);
void ioe_trace_kernel_entry(struct proc *, uint64);
void ioe_trace_scheduler(void);
void ioe_trace_idle(void);
void ioe_trace_heartbeat(void);
void ioe_trace_timer_interrupt(void);

#endif
