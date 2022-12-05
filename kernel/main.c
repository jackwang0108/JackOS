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
    console_put_str("main_pid: 0x"), console_put_int(sys_getpid()), console_put_char('\n');

    // 内核线程输出用户进程pid
    thread_start("k_thread_a", 31, k_thread_a, "argA ");
    thread_start("k_thread_b", 31, k_thread_b, "argB ");
    while(1);
    return 0;
}

void k_thread_a(void *arg){
    char* para = (char *)arg;
    console_put_str("thread_a_pid: 0x"), console_put_int(sys_getpid()), console_put_char('\n');
    while(1);
}

void k_thread_b(void *arg){
    char* para = (char *)arg;
    console_put_str("thread_b_pid: 0x"), console_put_int(sys_getpid()), console_put_char('\n');
    while(1);
}


void u_prog_a(void){
    char *name = "prog_a";
    printf("I am %s, my pid :%b%c", name, getpid(), '\n');
    while (1);
}


void u_prog_b(void){
    char *name = "prog_b";
    printf("I am %s, my pid :%d%c", name, getpid(), '\n');
    while(1);
}