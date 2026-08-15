#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "signal.h"

#define SIGNAL_FRAME_MAGIC 0x5349474652414d45UL
#define SUPPORTED_SIGNALS \
  ((1UL << SIGINT) | (1UL << SIGKILL) | (1UL << SIGSEGV) | \
   (1UL << SIGTERM) | (1UL << SIGCHLD))

struct signal_frame {
  uint64 magic;
  uint64 old_mask;
  struct signal_info info;
  struct trapframe saved;
};

extern struct proc proc[NPROC];
extern struct proc *initproc;
extern char trampoline[], sigtramp[];

static int
signal_valid(int sig)
{
  return sig > 0 && sig < NSIG && (SUPPORTED_SIGNALS & (1UL << sig));
}

static int
signal_default_ignored(int sig)
{
  return sig == SIGCHLD;
}

static int
signal_unblockable(int sig)
{
  return sig == SIGKILL;
}

static uint64
signal_sanitize_mask(uint64 mask)
{
  return mask & SUPPORTED_SIGNALS & ~(1UL << SIGKILL);
}

void
signal_init(struct proc *p)
{
  p->sig_pending = 0;
  p->sig_blocked = 0;
  memset(p->sig_actions, 0, sizeof(p->sig_actions));
  memset(p->sig_info, 0, sizeof(p->sig_info));
  p->sleep_interruptible = 0;
  p->term_signal = 0;
  p->last_signal = 0;
  p->pgid = p->pid;
  p->killed = 0;
}

void
signal_fork(struct proc *child, struct proc *parent)
{
  child->sig_pending = 0;
  child->sig_blocked = parent->sig_blocked;
  memmove(child->sig_actions, parent->sig_actions,
          sizeof(child->sig_actions));
  memset(child->sig_info, 0, sizeof(child->sig_info));
  child->sleep_interruptible = 0;
  child->term_signal = 0;
  child->last_signal = 0;
  child->pgid = parent->pgid;
  child->killed = 0;
}

void
signal_exec(struct proc *p)
{
  for(int sig = 1; sig < NSIG; sig++) {
    if(p->sig_actions[sig].handler != SIG_IGN)
      memset(&p->sig_actions[sig], 0, sizeof(p->sig_actions[sig]));
  }
  p->term_signal = 0;
  p->last_signal = 0;
}

static int
signal_send_locked(struct proc *p, int sig, int sender_pid, int code,
                   uint64 address, int force)
{
  if(sig == 0)
    return 1;
  if(!signal_valid(sig))
    return 0;

  struct sigaction *action = &p->sig_actions[sig];
  if(force) {
    p->sig_blocked &= ~(1UL << sig);
    if(action->handler == SIG_IGN)
      memset(action, 0, sizeof(*action));
  }

  if(action->handler == SIG_IGN ||
     (action->handler == SIG_DFL && signal_default_ignored(sig))) {
    ioe_trace_process(IOE_TRACE_SIGNAL_IGNORED, p, sig);
    return 0;
  }

  // Standard (non-real-time) signals coalesce. Linux keeps one pending bit.
  if((p->sig_pending & (1UL << sig)) == 0) {
    p->sig_info[sig].signo = sig;
    p->sig_info[sig].code = code;
    p->sig_info[sig].sender_pid = sender_pid;
    p->sig_info[sig].address = address;
  }
  p->sig_pending |= 1UL << sig;
  if(sig == SIGKILL)
    p->killed = 1;

  ioe_trace_process(IOE_TRACE_SIGNAL_GENERATED, p, sig);
  if(p->state == SLEEPING &&
     (p->sleep_interruptible || sig == SIGKILL))
    p->state = RUNNABLE;
  return 1;
}

int
signal_send_pid(int pid, int sig, int sender_pid, int code, uint64 address)
{
  if(sig != 0 && !signal_valid(sig))
    return -1;
  for(struct proc *p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state != UNUSED && p->pid == pid) {
      signal_send_locked(p, sig, sender_pid, code, address, 0);
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

int
signal_send_pgrp(int pgid, int sig, int sender_pid, int code)
{
  if(pgid <= 0 || (sig != 0 && !signal_valid(sig)))
    return -1;
  int found = 0;
  int actionable = 0;
  for(struct proc *p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state != UNUSED && p->pgid == pgid) {
      found++;
      actionable += signal_send_locked(p, sig, sender_pid, code, 0, 0);
    }
    release(&p->lock);
  }
  return found ? actionable : -1;
}

void
signal_force(struct proc *p, int sig, int code, uint64 address)
{
  acquire(&p->lock);
  signal_send_locked(p, sig, 0, code, address, 1);
  release(&p->lock);
}

int
signal_pending(struct proc *p)
{
  int pending;
  acquire(&p->lock);
  pending = (p->sig_pending & ~p->sig_blocked) != 0;
  release(&p->lock);
  return pending;
}

int
sleep_interruptible(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  release(lk);

  if((p->sig_pending & ~p->sig_blocked) == 0) {
    p->chan = chan;
    p->sleep_interruptible = 1;
    p->state = SLEEPING;
    sched();
  }

  p->chan = 0;
  p->sleep_interruptible = 0;
  int interrupted = (p->sig_pending & ~p->sig_blocked) != 0;
  release(&p->lock);
  acquire(lk);
  return interrupted;
}

static int
signal_next_locked(struct proc *p)
{
  uint64 ready = p->sig_pending & ~p->sig_blocked;
  if(ready & (1UL << SIGSEGV))
    return SIGSEGV;
  for(int sig = 1; sig < NSIG; sig++)
    if(ready & (1UL << sig))
      return sig;
  return 0;
}

static void
signal_restart_syscall(struct proc *p, uint64 flags)
{
  long result = (long)p->trapframe->a0;
  if(p->trapframe->trap_cause != 8)
    return;
  if(result == -ERESTARTSYS && (flags & SA_RESTART)) {
    p->trapframe->a0 = p->trapframe->orig_a0;
    p->trapframe->epc -= 4;
  } else if(result == -ERESTARTSYS || result == -ERESTARTNOHAND) {
    p->trapframe->a0 = (uint64)-EINTR;
  }
}

void
signal_deliver_pending(void)
{
  struct proc *p = myproc();
  struct sigaction action;
  struct signal_info info;
  int sig;

  acquire(&p->lock);
  sig = signal_next_locked(p);
  if(sig == 0) {
    release(&p->lock);
    return;
  }
  p->sig_pending &= ~(1UL << sig);
  p->last_signal = sig;
  p->killed = (p->sig_pending & (1UL << SIGKILL)) != 0;
  action = p->sig_actions[sig];
  info = p->sig_info[sig];
  memset(&p->sig_info[sig], 0, sizeof(p->sig_info[sig]));
  ioe_trace_process(IOE_TRACE_SIGNAL_DELIVERED, p, sig);

  if(action.handler == SIG_DFL) {
    if(signal_default_ignored(sig) || p == initproc) {
      ioe_trace_process(IOE_TRACE_SIGNAL_IGNORED, p, sig);
      release(&p->lock);
      return;
    }
    p->term_signal = sig;
    release(&p->lock);
    kexit(128 + sig);
  }

  if(action.handler == SIG_IGN) {
    ioe_trace_process(IOE_TRACE_SIGNAL_IGNORED, p, sig);
    release(&p->lock);
    return;
  }

  uint64 old_mask = p->sig_blocked;
  p->sig_blocked |= signal_sanitize_mask(action.mask);
  if((action.flags & SA_NODEFER) == 0)
    p->sig_blocked |= 1UL << sig;
  if(action.flags & SA_RESETHAND)
    memset(&p->sig_actions[sig], 0, sizeof(p->sig_actions[sig]));
  release(&p->lock);

  signal_restart_syscall(p, action.flags);
  struct signal_frame frame;
  frame.magic = SIGNAL_FRAME_MAGIC;
  frame.old_mask = old_mask;
  frame.info = info;
  frame.saved = *p->trapframe;
  uint64 sp = (p->trapframe->sp - sizeof(frame)) & ~15UL;
  if(sp >= p->trapframe->sp ||
     copyout(p->pagetable, sp, (char *)&frame, sizeof(frame)) < 0) {
    signal_force(p, SIGSEGV, SIGNAL_CODE_FAULT, sp);
    acquire(&p->lock);
    p->sig_actions[SIGSEGV].handler = SIG_DFL;
    p->sig_blocked &= ~(1UL << SIGSEGV);
    release(&p->lock);
    signal_deliver_pending();
    return;
  }

  p->trapframe->sp = sp;
  p->trapframe->epc = action.handler;
  p->trapframe->ra = SIGTRAMP + (sigtramp - trampoline);
  p->trapframe->a0 = sig;
  p->trapframe->a1 = sp + 2 * sizeof(uint64);
  p->trapframe->a2 = p->trapframe->a1 + sizeof(struct signal_info);
  ioe_trace_process(IOE_TRACE_SIGNAL_HANDLER, p, sig);
}

uint64
signal_sigreturn(void)
{
  struct proc *p = myproc();
  struct signal_frame frame;
  if(copyin(p->pagetable, (char *)&frame, p->trapframe->sp,
            sizeof(frame)) < 0 || frame.magic != SIGNAL_FRAME_MAGIC ||
     frame.saved.epc >= MAXVA || frame.saved.sp >= MAXVA)
    return -1;

  uint64 kernel_satp = p->trapframe->kernel_satp;
  uint64 kernel_sp = p->trapframe->kernel_sp;
  uint64 kernel_trap = p->trapframe->kernel_trap;
  uint64 kernel_hartid = p->trapframe->kernel_hartid;
  *p->trapframe = frame.saved;
  p->trapframe->kernel_satp = kernel_satp;
  p->trapframe->kernel_sp = kernel_sp;
  p->trapframe->kernel_trap = kernel_trap;
  p->trapframe->kernel_hartid = kernel_hartid;
  acquire(&p->lock);
  p->sig_blocked = signal_sanitize_mask(frame.old_mask);
  release(&p->lock);
  return p->trapframe->a0;
}

int
signal_sigaction(int sig, uint64 newaddr, uint64 oldaddr)
{
  if(!signal_valid(sig) || signal_unblockable(sig))
    return -1;
  struct proc *p = myproc();
  struct sigaction newaction;
  if(newaddr && copyin(p->pagetable, (char *)&newaction, newaddr,
                       sizeof(newaction)) < 0)
    return -1;
  acquire(&p->lock);
  struct sigaction oldaction = p->sig_actions[sig];
  if(newaddr) {
    newaction.mask = signal_sanitize_mask(newaction.mask);
    p->sig_actions[sig] = newaction;
    if(newaction.handler == SIG_IGN) {
      p->sig_pending &= ~(1UL << sig);
      memset(&p->sig_info[sig], 0, sizeof(p->sig_info[sig]));
    }
  }
  release(&p->lock);
  if(oldaddr && copyout(p->pagetable, oldaddr, (char *)&oldaction,
                        sizeof(oldaction)) < 0)
    return -1;
  return 0;
}

int
signal_sigprocmask(int how, uint64 setaddr, uint64 oldaddr)
{
  struct proc *p = myproc();
  uint64 set = 0;
  if(setaddr && copyin(p->pagetable, (char *)&set, setaddr, sizeof(set)) < 0)
    return -1;
  acquire(&p->lock);
  uint64 old = p->sig_blocked;
  if(setaddr) {
    if(how == SIG_BLOCK)
      p->sig_blocked |= set;
    else if(how == SIG_UNBLOCK)
      p->sig_blocked &= ~set;
    else if(how == SIG_SETMASK)
      p->sig_blocked = set;
    else {
      release(&p->lock);
      return -1;
    }
    p->sig_blocked = signal_sanitize_mask(p->sig_blocked);
  }
  release(&p->lock);
  if(oldaddr && copyout(p->pagetable, oldaddr, (char *)&old, sizeof(old)) < 0)
    return -1;
  return 0;
}

int
signal_setpgid(int pid, int pgid)
{
  struct proc *caller = myproc();
  if(pid == 0)
    pid = caller->pid;
  if(pgid == 0)
    pgid = pid;
  if(pid < 1 || pgid < 1)
    return -1;
  for(struct proc *p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state != UNUSED && p->pid == pid) {
      if(p != caller && p->parent != caller) {
        release(&p->lock);
        return -1;
      }
      p->pgid = pgid;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}
