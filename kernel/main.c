#include "init.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "print.h"
#include "console.h"
#include "keyboard.h"
#include "process.h"
#include "syscall-init.h"
#include "syscall.h"
#include "stdio.h"
#include "kstdio.h"


void k_thread_a(void *);
void k_thread_b(void *);
void u_prog_a(void);
void u_prog_b(void);



int main(void){
    put_str("In kernel now, start happy c time!\n");
    init_all();

    // 用户进程调用系统调用
    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");

    // 开中断, 使得时钟中断可以运行, 从而能够开始运行线程调度
    intr_enable();
    kprintf("main_pid: 0x%x\n", sys_getpid());

    // 内核线程输出用户进程pid
    thread_start("k_thread_a", 31, k_thread_a, "argA ");
    thread_start("k_thread_b", 31, k_thread_b, "argB ");
    while(1);
    return 0;
}

void k_thread_a(void *arg){
    char* para = (char *)arg;

    void *addrs[7];
    kprintf("thread a_start\n");
    int max = 1000;
    while (max-- > 0){
        int i, size;
        // alloc test 1
        for (i = 0, size = 128; i < 4; i++, size *= 2){
            addrs[i] = sys_malloc(size);
            if (i == 2)
                sys_free(addrs[i]);
        }
        // alloc test 2
        size <<= 7;                     // equals to size *= 2 * 2 * 2 * 2 * 2 * 2 * 2
        addrs[i++] = sys_malloc(size);
        addrs[i++] = sys_malloc(size);
        sys_free(addrs[i - 1]);          // i = 5

        // alloc test 3
        size *= 2;
        addrs[i++] = sys_malloc(size);

        // release all
        while(i-- > 0 && i != 2 && i != 5)
            sys_free(addrs[i]);
    }
    kprintf("thread a_ends\n");
    while(1);
}

void k_thread_b(void *arg){
    char* para = (char *)arg;
    void *addrs[9];
    int max = 1000;
    kprintf("thread b_start\n");
    while (max-- > 0){
        int i = 0, size = 9;
        // alloc test 1
        for (i = 0; i < 2; i++, size *= 2)
            addrs[i] = sys_malloc(size);
        sys_free(addrs[i - 1]);              // i = 1

        // alloc test 2
        addrs[i++] = sys_malloc(size);
        sys_free(addrs[0]);                  // i = 0
        
        // alloc test 3
        for (; i < 6; i++)
            addrs[i] = sys_malloc(size);
        sys_free(addrs[4]);                  // i = 4

        // alloc test 4
        addrs[6] = sys_malloc((size*=2));
        sys_free(addrs[2]);
        sys_free(addrs[3]);
        sys_free(addrs[6]);
        sys_free(addrs[5]);

        // alloc test 5
        size <<= 3;
        for (i = 0; i < 9; i++)
            addrs[i] = sys_malloc(size);

        for (i = 0; i < 9; i++)
            sys_free(addrs[i]);
    }
    kprintf("thread_b end\n");
    while(1);
}



void u_prog_a(void){
    void *vaddrs[3] = {
        malloc(256),
        malloc(255),
        malloc(254)
    };
    printf("prog_a_malloc addr: 0x%x, 0x%x, 0x%x\n", (void*)vaddrs[0], (void*)vaddrs[1], (void*)vaddrs[2]);
    int delay = 1e5;
    while (delay-- > 0);
    for (int i = 0; i < 3; i++)
        free(vaddrs[i]);
    while (1);
}


void u_prog_b(void){
    void *vaddrs[3] = {
        malloc(256),
        malloc(255),
        malloc(254)
    };
    printf("prog_b_malloc addr: 0x%x, 0x%x, 0x%x\n", (void*)vaddrs[0], (void*)vaddrs[1], (void*)vaddrs[2]);
    int delay = 1e5;
    while (delay-- > 0);
    for (int i = 0; i < 3; i++)
        free(vaddrs[i]);
    while (1);
}