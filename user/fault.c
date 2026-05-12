// Test that the kernel correctly handles page faults. 
// This program should crash with a page fault when it tries to access the invalid pointer 0xdeadbeef.
// Code reference: https://www.rose-hulman.edu/class/csse/csse332/2425b/labs/cow/

#include <stdio.h>
#include <stdlib.h>
int
main(int argc, char **argv)
{
  char *p = (char *)0xdeadbeef;

  /* Try to access an invalid pointer and check what happens. */
  *p = 0xff;
  printf("The ptr %p points to %c", p, *p);

  exit(0);
}


