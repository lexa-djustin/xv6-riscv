#include "kernel/types.h"
#include "user/user.h"
#include "user/net.h"

int main(int volatile argc, char *argv[]) {
  int fd = socket(SOCK_DGRAM);

  int pid = getpid();
  int port = atoi(argv[1]);
  char buffer[256];

  if (port == 0) {
    port = 8080;
  }

  bind(fd, port);

  while (1) {
    read(fd, buffer, sizeof(buffer));

    printf("pid: %d; port: %d; data: %s\n", pid, port, buffer);
    memset(buffer, 0, sizeof(buffer));
  }

  return 0;
}
