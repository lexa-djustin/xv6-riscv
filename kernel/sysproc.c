#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if (t == SBRK_EAGER || n < 0) {
    if (growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > SIGTRAMP)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (sleep_interruptible(&ticks, &tickslock)) {
      release(&tickslock);
      return -ERESTARTNOHAND;
    }
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid, sig;
  struct proc *p = myproc();
  argint(0, &pid);
  argint(1, &sig);
  if(pid > 0)
    return signal_send_pid(pid, sig, p->pid, SIGNAL_CODE_USER, 0);
  if(pid == 0) {
    int result = signal_send_pgrp(p->pgid, sig, p->pid, SIGNAL_CODE_USER);
    return result < 0 ? -1 : 0;
  }
  if(pid < -1) {
    int result = signal_send_pgrp(-pid, sig, p->pid, SIGNAL_CODE_USER);
    return result < 0 ? -1 : 0;
  }
  return -1;
}

uint64
sys_sigaction(void)
{
  int sig;
  uint64 action, oldaction;
  argint(0, &sig);
  argaddr(1, &action);
  argaddr(2, &oldaction);
  return signal_sigaction(sig, action, oldaction);
}

uint64
sys_sigprocmask(void)
{
  int how;
  uint64 set, oldset;
  argint(0, &how);
  argaddr(1, &set);
  argaddr(2, &oldset);
  return signal_sigprocmask(how, set, oldset);
}

uint64
sys_sigreturn(void)
{
  return signal_sigreturn();
}

uint64
sys_setpgid(void)
{
  int pid, pgid;
  argint(0, &pid);
  argint(1, &pgid);
  return signal_setpgid(pid, pgid);
}

uint64
sys_getpgrp(void)
{
  return myproc()->pgid;
}

extern int cons_foreground_pgid;

uint64
sys_tcsetpgrp(void)
{
  int fd, pgid;
  argint(0, &fd);
  argint(1, &pgid);
  if(fd < 0 || pgid < 1 ||
     signal_send_pgrp(pgid, 0, myproc()->pid, SIGNAL_CODE_USER) < 0)
    return -1;
  cons_foreground_pgid = pgid;
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
