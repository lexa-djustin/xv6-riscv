#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{

  while (1) {
    int n = 0;
  
    while (n++ < 1000000000);

    printf("Hello\n");
  }
  
  exit(0);
}
