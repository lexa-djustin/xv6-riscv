#include "kernel/types.h"
#include "user/user.h"
#include "user/net.h"


int main(int volatile argc, char *argv[]) {
  int fd = socket(SOCK_STREAM);

//  printf("res: %d\n", res);

  int connected = connect(fd, 12345, 0xC1A80102);

  printf("connected; res: %d\n", connected);

//  write(fd, "Hello from xv6 #1!", 18);
//  write(fd, "Hello from xv6 #2!", 18);
//  write(fd, "Hello from xv6 #3!", 18);

//  write(fd, "#1 !", 4);
//  write(fd, "#2 !", 4);
//  write(fd, "#3 !", 4);

  char c = 'a';
  int sent = 0;

  while (1) {
    write(fd, &c, 1);
    sent++;

    printf("sent: %d\n", sent);

    if (c == 'z') {
      break;
    }

    c++;

//    write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 216);
    sleep(1);
  }

//  sleep(5);

  close(fd);


//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 54);
//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 54);





//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 54);
//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 54);
//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 216);
//  sleep(10);
//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 216);
//  sleep(10);
//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 216);
//  sleep(10);
//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 216);
//  sleep(10);
//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 216);
//  sleep(10);
//  write(fd, "Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!Hello from xv6 #1!Hello from xv6 #2!Hello from xv6 #3!", 216);
//  sleep(10);

  return 0;
}
