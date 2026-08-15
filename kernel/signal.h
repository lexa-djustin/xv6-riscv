#ifndef XV6_SIGNAL_H
#define XV6_SIGNAL_H

// Linux signal numbers for the subset exposed by Interactive OS Explorer.
#define SIGINT  2
#define SIGKILL 9
#define SIGSEGV 11
#define SIGTERM 15
#define SIGCHLD 17
#define NSIG    18

#define SIG_DFL ((uint64)0)
#define SIG_IGN ((uint64)1)

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

// Values match Linux so examples using the flags remain familiar.
#define SA_RESTART   0x10000000UL
#define SA_NODEFER   0x40000000UL
#define SA_RESETHAND 0x80000000UL

#define EINTR          4
#define ERESTARTSYS    512
#define ERESTARTNOHAND 514

#define SIGNAL_CODE_USER   0
#define SIGNAL_CODE_TTY    1
#define SIGNAL_CODE_KERNEL 2
#define SIGNAL_CODE_FAULT  3
#define SIGNAL_CODE_CHILD  4

struct sigaction {
  uint64 handler;
  uint64 mask;
  uint64 flags;
};

struct signal_info {
  int signo;
  int code;
  int sender_pid;
  int reserved;
  uint64 address;
};

#endif
