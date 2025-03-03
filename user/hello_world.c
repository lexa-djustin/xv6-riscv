#include "kernel/types.h"
#include "user/user.h"

int main(int volatile argc, char *argv[]) {
  write(1, "Hello, world!\n", 14);

  return 0;
}
