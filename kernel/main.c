#include "init.h"
#include "syscall-init.h"
#include "syscall.h"
#include "kstdio.h"
#include "print.h"
#include "test.h"



int main(void){
    put_str("In kernel now, start happy c time!\n");
    init_all();


    // 开中断, 使得时钟中断可以运行, 从而能够开始运行线程调度
    // intr_enable();
    kprintf("main_pid: 0x%x\n", sys_getpid());

    test_all();

    while(1);
    return 0;
}


