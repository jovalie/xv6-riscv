#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// helper function to count number of processes
// reference: https://stackoverflow.com/questions/57745250/how-to-add-a-system-call-to-find-the-number-of-processes-in-xv6
static int
count_processes(void)
{
  int n = 0;
  // arracy proc is global process table.
  // constant NPROC is the maximum number of processes in the system.
  for(struct proc *p = proc; p < &proc[NPROC]; p++){

    // locking the process before checking its state
    // without this, another CPU could change the state (e.g., from RUNNABLE to UNUSED) while it is being read
    acquire(&p->lock);
    if(p->state != UNUSED)
      n++;
    release(&p->lock);
  }

  return n;
}

uint64
sys_info(void)
{
  int param;
  struct proc *p = myproc();
  argint(0, &param);

  p->info_calls++;

  // Option 1: A count of the processes in the system
  if(param == 1){
    return count_processes();
  }
  // Option 2: return the number of times this process has called info()
  else if (param == 2){
    return p->info_calls;
  }
  // Option 3: The number of memory pages current process is using above address 0x8000000 (128 MB)
  else if (param == 3){
    uint64 floor = 0x8000000;
    // printf("Size of process memory (bytes): %ld\n", p->sz);
    if (p->sz <= floor) {
      return 0;
    }
    uint64 bytes_above = p->sz - floor;
    return (PGROUNDUP(bytes_above)) / PGSIZE;
  }
  // Option 4: The address of the kernel stack for the current process
  else if (param == 4){
    return p->kstack;
  }
  return -1;
}
