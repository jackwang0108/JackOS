#include "init.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "print.h"
#include "console.h"
#include "keyboard.h"

void k_thread_a(void *);
void k_thread_b(void *);

int main(void){
    put_str("In kernel now, start happy c time!\n");
    init_all();

    // 测试调度多线程调度
    thread_start("consumer_a", 31, k_thread_a, " A_");
    thread_start("consumer_b", 31, k_thread_b, " B_");

    // 开中断
    intr_enable();
    while(1);
    return 0;
}

void k_thread_a(void *arg){
    while (1){
        intr_status_t old_status = intr_disable();
        if (!ioq_empty(&kbd_buf)){
            console_put_str((char*)arg);
            char byte = ioq_getchar(&kbd_buf);
            console_put_char(byte);
        }
        intr_set_status(old_status);
    }
}

void k_thread_b(void *arg){
    while (1){
        intr_status_t old_status = intr_disable();
        if (!ioq_empty(&kbd_buf)){
            console_put_str((char*)arg);
            char byte = ioq_getchar(&kbd_buf);
            console_put_char(byte);
        }
        intr_set_status(old_status);
    }
}