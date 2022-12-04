#include "timer.h"
#include "io.h"
#include "print.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"

#define IRQ0_FREQUENCY              100
#define INPUT_FREQUENCY             1193180
#define COUNTER0_VALUE              INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT               0x40
#define COUNTER0_NO                 0
#define COUNTER_MODE                2
#define READ_WRITE_LATCH            3
#define PIT_CONTROL_PORT            0x43

uint32_t ticks;                     // ticks是自从内核开始运行后，开启中断以来总的tick数


/**
 * @brief frequency_set 用于初始化8253可编程定时计数器
 * 
 * @param counter_port 控制字寄存器端口，只能是0x43
 * @param counter_no 8253计数器上有三组独立的计数器，该参数指定了将要初始化哪组寄存器
 * @param rwl 指定计数器的读写方式
 * @param counter_mode 指定计数器的工作模式
 * @param counter_value 指定计数器的初值
 */
static void frequency_set(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value){
    // 写入控制字寄存器
    outb(PIT_CONTROL_PORT, (uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));
    // 设置初值
    outb(counter_port, (uint8_t) counter_value);                // 先写入低八位
    outb(counter_port, (uint8_t) counter_value >> 8);         // 然后写入高八位
}


/**
 * @brief 时钟中断处理函数
 * 
 */
void intr_timer_handler(void){
    // 获得当前正在运行的进程/线程的PCB/TCB
    task_struct_t *cur_thread = running_thread();
    // 检查栈是否溢出，若栈溢出，则stack_magic的值会被改变
    ASSERT(cur_thread->stack_magic == 0x20010107)
    // 当前线程的总tick数+1
    cur_thread->total_ticks++;
    // 内核总tick数+1
    ticks++;

    if (cur_thread->this_tick == 0)
        schedule();                 // 当前线程的时间片已经用完了，则调度新的进程上CPU
    else
        cur_thread->this_tick--;
    
}


/**
 * @brief timer_init用于初始化8253可编程计时器来提供系统的时钟中断
 * 
 */
void timer_init(void){
    put_str("timer_init start\n");
    frequency_set(
        COUNTER0_PORT,
        COUNTER0_NO,
        READ_WRITE_LATCH,
        COUNTER_MODE,
        COUNTER0_VALUE
    );
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done\n");
}