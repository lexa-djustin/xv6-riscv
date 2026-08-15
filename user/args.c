#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("args: argc=%d\n", argc);
  for(int index = 0; index < argc; index++)
    printf("args: argv[%d]=%s\n", index, argv[index]);
  exit(0);
}
