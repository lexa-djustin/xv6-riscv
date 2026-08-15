#include "kernel/types.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define DEFAULT_PAGES 2048

int
main(int argc, char *argv[])
{
  int pages = DEFAULT_PAGES;

  if(argc > 1){
    pages = atoi(argv[1]);
    if(pages < 1){
      fprintf(2, "usage: kerneltime [positive-pages]\n");
      exit(1);
    }
  }

  printf("kerneltime: allocating %d pages through the kernel\n", pages);
  for(int page_index = 0; page_index < pages; page_index++){
    char *page = sbrk(PAGE_SIZE);
    if(page == (char *)-1){
      fprintf(2, "kerneltime: sbrk failed at page %d\n", page_index + 1);
      exit(1);
    }
    page[0] = (char)page_index;
  }
  printf("kerneltime: done\n");
  exit(0);
}
