// very similar to the simple test in cowtest.c, however it does not call the wait system call since we haven’t handled yet what happens when a process dies.

#include "kernel/types.h"
#include "kernel/memlayout.h"
#include "user/user.h"

void
simpletest()
{
    // calculates the total amount of usable physical memory (RAM) available to user processes in this
    // operating system.
    uint64 totalmem = PHYSTOP - KERNBASE;
    // calculates 55% of the total usable physical memory.
    int siz = 0.55 * totalmem;
    printf("simple: ");
    // allocating memory
    char *p = sbrk(siz);
    if (p == (char*) -1) {
        printf("sbrk(%d) failed\n", siz);
        exit(-1);
    }
    // go through the allocated memory and write 'X' to each page. we ensure that the pages are actually allocated and to test the copy-on-write behavior during fork.
    for(char *q = p; q < p + siz; q += 4096){
        *q = 'X';
    }
    // right now the parent process is using 55% of the system's entire physical RAM
    // Suceeds only if CoW is implemented
    int pid = fork();
    if(pid < 0){
        printf("fork() failed\n");
        exit(-1);
    }
    // child process (pid == 0) immediately exits without writing to any memory.
    if(pid == 0)
        exit(0);
    // parent releases the 55% memory it allocated by calling sbrk() with a negative size
    if(sbrk(-siz) == (char*) -1){
        printf("sbrk(-%d) failed\n", siz);
        exit(-1);
    }
    printf("ok\n");
}

int
main(int argc, char *argv[])
{
    simpletest();

    // check that the first simpletest() freed the physical memory.
    simpletest();
    
    printf("ALL COW TESTS PASSED\n");
    
    exit(0);
}

