#include "kernel/types.h"
#include "user/user.h"
#include "user/net.h"


int main(int volatile argc, char *argv[]) {
  int fd = socket(SOCK_DGRAM);

  connect(fd, 8080, 0xC1A80102);
  write(fd, "Hello from xv6!", 15);

  return 0;
}
