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

    // use function to simulate user program
    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");

    // enable interrupt, so that 9253 timer functions and schedule() start to schedule user program
    intr_enable();
    console_put_str("main_pid: 0x"), console_put_int(sys_getpid()), console_put_char('\n');

    // kernel thread
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

void u_prog_a(void){
    char *name = "prog_a";
    printf("I am %s, my pid: 0x%x%c", name, getpid(), '\n');
    while (1);
}

void k_thread_b(void *arg){
    char* para = (char *)arg;
    console_put_str("thread_b_pid: 0x"), console_put_int(sys_getpid()), console_put_char('\n');
    while(1);
}



void u_prog_b(void){
    char *name = "prog_b";
    printf("I am %s, my pid: 0x%d%c", name, getpid(), '\n');
    while(1);
}