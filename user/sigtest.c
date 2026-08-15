#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/signal.h"
#include "user/user.h"

static volatile int handled;

static void handler(int, struct signal_info *, void *);
static void fail(char *);

int
main(void)
{
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.handler = (uint64)handler;
  action.flags = SA_RESTART;
  if(sigaction(SIGINT, &action, 0) < 0)
    fail("sigaction");

  uint64 mask = 1UL << SIGINT;
  if(sigprocmask(SIG_BLOCK, &mask, 0) < 0)
    fail("block");
  if(kill(getpid(), SIGINT) < 0)
    fail("kill self");
  if(kill(getpid(), SIGINT) < 0)
    fail("coalesced kill self");
  if(handled)
    fail("blocked signal delivered too early");
  if(sigprocmask(SIG_UNBLOCK, &mask, 0) < 0)
    fail("unblock");
  pause(0);
  if(handled != 1)
    fail("handler/coalescing");

  if(sigaction(SIGKILL, &action, 0) == 0)
    fail("SIGKILL became catchable");

  int fds[2];
  if(pipe(fds) < 0)
    fail("pipe");
  handled = 0;
  int parent = getpid();
  int writer = fork();
  if(writer < 0)
    fail("restart fork");
  if(writer == 0) {
    close(fds[0]);
    pause(2);
    kill(parent, SIGINT);
    pause(2);
    write(fds[1], "x", 1);
    close(fds[1]);
    exit(0);
  }
  close(fds[1]);
  char byte = 0;
  if(read(fds[0], &byte, 1) != 1 || byte != 'x')
    fail("SA_RESTART read");
  close(fds[0]);
  if(handled != 1)
    fail("interruptible read handler");
  wait(0);

  int pid = fork();
  if(pid < 0)
    fail("fork");
  if(pid == 0) {
    pause(1000000);
    exit(2);
  }
  if(kill(pid, SIGTERM) < 0)
    fail("SIGTERM");
  int status = 0;
  if(wait(&status) != pid || status != 128 + SIGTERM)
    fail("termination status");

  printf("sigtest: OK\n");
  exit(0);
}

static void
handler(int sig, struct signal_info *info, void *context)
{
  (void)context;
  if(sig == SIGINT && info->signo == SIGINT)
    handled++;
}

static void
fail(char *message)
{
  fprintf(2, "sigtest: FAIL: %s\n", message);
  exit(1);
}
