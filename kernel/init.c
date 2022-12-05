#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"

void init_all(void){
    put_str("init_all\n");
    idt_init();                 // 初始化中断描述符表
    mem_init();                 // 初始化内存管理系统，包括虚拟内存和物理内存
    thread_init();              // 初始化线程，为内核构建主线程
    timer_init();               // 初始化PIT（Programmable Interval Timer）
    console_init();             // 初始化控制台
    keyboard_init();            // 初始化键盘
    tss_init();
    syscall_init();
}