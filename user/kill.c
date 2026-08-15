#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/signal.h"
#include "user/user.h"

static int
signal_number(char *name)
{
  if(name[0] == '-')
    name++;
  if(name[0] >= '0' && name[0] <= '9')
    return atoi(name);
  if(strcmp(name, "INT") == 0 || strcmp(name, "SIGINT") == 0)
    return SIGINT;
  if(strcmp(name, "KILL") == 0 || strcmp(name, "SIGKILL") == 0)
    return SIGKILL;
  if(strcmp(name, "SEGV") == 0 || strcmp(name, "SIGSEGV") == 0)
    return SIGSEGV;
  if(strcmp(name, "TERM") == 0 || strcmp(name, "SIGTERM") == 0)
    return SIGTERM;
  if(strcmp(name, "CHLD") == 0 || strcmp(name, "SIGCHLD") == 0)
    return SIGCHLD;
  return -1;
}

int
main(int argc, char **argv)
{
  int sig = SIGTERM;
  int first_pid = 1;
  if(argc >= 3 && strcmp(argv[1], "-s") == 0) {
    sig = signal_number(argv[2]);
    first_pid = 3;
  } else if(argc >= 2 && argv[1][0] == '-') {
    sig = signal_number(argv[1]);
    first_pid = 2;
  }
  if(sig < 0 || first_pid >= argc) {
    fprintf(2, "usage: kill [-s SIGNAL|-SIGNAL] pid...\n");
    exit(1);
  }
  int failed = 0;
  for(int i = first_pid; i < argc; i++) {
    int pid = atoi(argv[i]);
    if(kill(pid, sig) < 0) {
      fprintf(2, "kill: cannot signal %d\n", pid);
      failed = 1;
    }
  }
  exit(failed);
}
