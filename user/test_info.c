#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    printf("Total number of info system calls made by current process: %d\n", info(2));
    printf("Total number of processes: %d\n", info(1));
    printf("Total number of info system calls made by current process: %d\n", info(2));
    printf("Total number of memory pages above 0x8000000 used by current process: %d\n", info(3));
    void *prev_break = sbrk(140 * 1024 * 1024); // allocation 140 MB
    if(prev_break == (void*)-1) {
        printf("Error: sbrk failed to allocate 140MB. System out of memory.\n");
    } else {
        printf("Success: Memory allocated.\n");
    }
    printf("Total number of memory pages above 0x8000000 used by current process: %d\n", info(3));
    printf("Total number of info system calls made by current process: %d\n", info(2));
    printf("Address of the kernel stack: 0x%x\n", info(4));
    exit(0);
}