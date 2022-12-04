#include "init.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "print.h"
#include "console.h"
#include "keyboard.h"
#include "process.h"

void k_thread_a(void *);
void k_thread_b(void *);
void u_prog_a(void);
void u_prog_b(void);

int test_var_a = 0, test_var_b = 0;

int main(void){
    put_str("In kernel now, start happy c time!\n");
    init_all();

    // 测试调度多线程调度
    thread_start("consumer_a", 31, k_thread_a, " A_");
    thread_start("consumer_b", 31, k_thread_b, " B_");
    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");

    // 开中断
    intr_enable();
    while(1);
    return 0;
}

void k_thread_a(void *arg){
    char* para = (char *)arg;
    while (1){
        console_put_str(" v_a: 0x");
        console_put_int(test_var_a);
        console_put_str("\n");
    }
}

void k_thread_b(void *arg){
    char* para = (char *)arg;
    while (1){
        console_put_str(" v_b: 0x");
        console_put_int(test_var_b);
        console_put_str("\n");
    }
}


void u_prog_a(void){
    while (1)
        test_var_a++;
}


void u_prog_b(void){
    while (1)
        test_var_b ++;
}