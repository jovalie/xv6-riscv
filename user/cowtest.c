// tests for CoW

// reference: https://gitlab.cs.washington.edu/csep551/xv6-19au/-/blob/b36276d10137e7d8b73a7e72e290dd5d4770e125/user/cowtest.c


#include "kernel/types.h"
#include "kernel/memlayout.h"
#include "user/user.h"

enum {
  // page size
  PGSZ = 4096,
};

// simple test for copy-on-write fork.
// allocates more than half of available physical memory for one process, and then calls fork().
void
simpletest()
{
    // computes total amount of usable physical memory available to user processes.
    uint64 totalmem = PHYSTOP - KERNBASE;

    // computes 55% of usable physical memory.
    int siz = 0.55 * totalmem;

    printf("simple: ");
  
    // allocates memory.
    char *p = sbrk(siz);
    if (p == (char*) -1) {
        printf("sbrk(%d) failed\n", siz);
        exit(-1);
    }

    // touches each page so pages exist before fork.
    for(char *q = p; q < p + siz; q += 4096){
        *q = 'X';
    }

    // succeeds only if cow is implemented.
    int pid = fork();
    if(pid < 0){
        printf("fork() failed\n");
        exit(-1);
    }

    // child exits without writing shared memory.
    if(pid == 0)
        exit(0);
    wait(0);

    // parent releases memory with negative sbrk.
    if(sbrk(-siz) == (char*) -1){
        printf("sbrk(-%d) failed\n", siz);
        exit(-1);
    }

    printf("ok\n");
}

// creates three related processes that all write into shared cow memory.
// uses enough memory to pressure allocator and to check whether
// copied pages are released after writes and exits.
void
threetest()
{
  uint64 phys_size = PHYSTOP - KERNBASE;
  int sz = phys_size / 4;
  int pid1, pid2, xstatus;

  printf("three: ");
  
  // allocates a large region so later writes will create many private pages.
  char *p = sbrk(sz);
  if(p == (char*) -1){
    printf("sbrk(%d) failed\n", sz);
    exit(-1);
  }

  // first fork creates a child that will later fork again.
  pid1 = fork();
  if(pid1 < 0){
    printf("fork failed\n");
    exit(-1);
  }
  if(pid1 == 0){
    // second fork creates a grandchild so two descendants can both
    // write into shared pages in different ways.
    pid2 = fork();
    if(pid2 < 0){
      printf("fork failed");
      exit(-1);
    }
    if(pid2 == 0){
      // grandchild writes most pages with its pid and then checks
      // that every write stayed private to its own address space.
      for(char *q = p; q < p + (sz/5)*4; q += 4096){
        *(int*)q = getpid();
      }
      for(char *q = p; q < p + (sz/5)*4; q += 4096){
        if(*(int*)q != getpid()){
          printf("wrong content\n");
          exit(-1);
        }
      }
      exit(0);
    }
    // child writes a different pattern into half of region so parent,
    // child, and grandchild all force separate cow copies.
    for(char *q = p; q < p + (sz/2); q += 4096){
      *(int*)q = 9999;
    }
    // child waits for grandchild so this test fails if grandchild
    // observes wrong data or crashes.
    wait(&xstatus);
    if(xstatus != 0){
      exit(-1);
    }
    exit(0);
  }

  // original parent writes its own pid into every page before
  // descendants begin modifying shared mappings.
  for(char *q = p; q < p + sz; q += 4096){
    *(int*)q = getpid();
  }

  // parent waits for child branch to finish its writes.
  wait(0);

  // gives exiting descendants a moment to release private pages.
  sleep(1);

  // parent checks that its original values still exist everywhere.
  for(char *q = p; q < p + sz; q += 4096){
    if(*(int*)q != getpid()){
      printf("wrong content\n");
      exit(-1);
    }
  }

  if(sbrk(-sz) == (char*) -1){
    printf("sbrk(-%d) failed\n", sz);
    exit(-1);
  }

  printf("ok\n");
}


// filler arrays for filetest
char junk1[4096];
int fds[2];
char junk2[4096];
char buf[4096];
char junk3[4096];

// checks whether kernel copyout path handles cow pages correctly.
// child reads from pipe into shared buffer so kernel must create
// a private writable copy without changing parent buffer.
void
filetest()
{
  printf("file: ");
  
  // parent keeps a sentinel byte so test can detect whether child
  // read path changed parent buffer.
  buf[0] = 99;

  // exercise copyout more than once on different child processes.
  for(int i = 0; i < 4; i++){
    if(pipe(fds) != 0){
      printf("pipe() failed\n");
      exit(-1);
    }
    int pid = fork();
    if(pid < 0){
      printf("fork failed\n");
      exit(-1);
    }
    if(pid == 0){
      // child reads an integer into shared buffer.
      sleep(1);
      if(read(fds[0], buf, sizeof(i)) != sizeof(i)){
        printf("error: read failed\n");
        exit(1);
      }
      // child checks that value copied through kernel matches
      // what parent wrote into pipe.
      sleep(1);
      int j = *(int*)buf;
      if(j != i){
        printf("error: read the wrong value\n");
        exit(1);
      }
      exit(0);
    }
    // parent writes one integer into pipe for child to read.
    if(write(fds[1], &i, sizeof(i)) != sizeof(i)){
      printf("error: write failed\n");
      exit(-1);
    }
  }

  int xstatus = 0;
  // waits for every child and fails if any child saw bad data.
  for(int i = 0; i < 4; i++) {
    wait(&xstatus);
    if(xstatus != 0) {
      exit(1);
    }
  }

  // parent verifies its original sentinel byte stayed untouched.
  if(buf[0] != 99){
    printf("error: child overwrote parent\n");
    exit(1);
  }

  printf("ok\n");
}

void
isolationtest()
{
  int xstatus;
  int sz = 2 * PGSZ;

  // correctness test:
  // allocates two pages and uses child writes to verify isolation.
  printf("isolation: ");

  // allocates memory for two full pages.
  char *p = sbrk(sz);
  if(p == (char*)-1){
    printf("sbrk(%d) failed\n", sz);
    exit(1);
  }

  // fills memory with known bytes before fork so later checks can
  // detect whether child writes leaked back into parent.
  memset(p, 'a', sz);
  p[0] = 'A';
  p[PGSZ - 1] = 'B';
  p[PGSZ] = 'C';
  p[sz - 1] = 'D';

  int pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // child writes into both pages and verifies its own private view.
    p[0] = 'x';
    p[PGSZ - 1] = 'y';
    p[PGSZ] = 'z';
    p[sz - 1] = 'w';

    if(p[0] != 'x' || p[PGSZ - 1] != 'y' || p[PGSZ] != 'z' || p[sz - 1] != 'w'){
      printf("child sees wrong private data\n");
      exit(1);
    }
    exit(0);
  }

  // parent waits for child and checks original bytes stayed unchanged.
  wait(&xstatus);
  if(xstatus != 0){
    printf("child failed\n");
    exit(1);
  }

  if(p[0] != 'A' || p[PGSZ - 1] != 'B' || p[PGSZ] != 'C' || p[sz - 1] != 'D'){
    printf("child write leaked into parent\n");
    exit(1);
  }

  // parent writes afterward to make sure its own mapping still works.
  p[1] = 'Q';
  if(p[1] != 'Q' || p[0] != 'A' || p[PGSZ] != 'C'){
    printf("parent lost private copy\n");
    exit(1);
  }

  if(sbrk(-sz) == (char*)-1){
    printf("sbrk(-%d) failed\n", sz);
    exit(1);
  }

  printf("ok\n");
}

// security test:
// checks that fork does not make text pages writable.
void
texttest()
{
  int pid;
  int xstatus;

  printf("text: ");

  // child tries to write its own text segment and should fault.
  pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    exit(1);
  }

  if(pid == 0){
    volatile char *p = (char*)texttest;
    *p = 1;
    printf("was able to write text\n");
    exit(1);
  }

  wait(&xstatus);
  if(xstatus == 0){
    printf("text page became writable\n");
    exit(1);
  }

  printf("ok\n");
}

int
main(int argc, char *argv[])
{
  simpletest();

  // checks that first simpletest() released physical memory before
  // running same test
  simpletest();

  // multi-process write test
  // making sure performance is predictable across multiple runs
  threetest();
  threetest();
  threetest();

  // kernel copyout behavior, page isolation,
  // parent exit handling, and text-page protection.
  filetest();
  isolationtest();
  texttest();

  printf("ALL COW TESTS PASSED\n");

  exit(0);
}
