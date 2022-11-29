/**
 * @file thread.h
 * @author Shihong Wang
 * @brief thread module of the kernel
 * @version 0.1
 * @date 2022-11-26
 * 
 * @copyright Copyright (c) 2022
 * 
 * @note 本系统中，调度器最小的调度对象就是线程，进程和线程最大的区别就是进程拥有独立的虚拟内存
 *      因此，本文件中将支持多进程并发的库称为thread
 */

#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "stdint.h"
#include "list.h"


typedef void thread_func(void*);


typedef enum __task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
} task_status_t;


// 中断发生时的调用流程：
//      1. 外部中断到8259A
//      2. 8259A通知CPU发生了某个中断
//      3. CPU收到8259A发来的消息，得知发生中断
//      4. CPU发消息到8259A以获得中断的中断号
//      5. 8259A收到CPU的消息，将中断号发送给CPU
//      6. CPU收到中断号，将确认收到消息发送给8259A
//      7. CPU从idt中根据中断号去获取中断门
//      8. 中断门中保存了中断处理程序中断处理程序的地址，中断门权限，目的表代码段权限，这个中断处理程序地址在kernerl.S中，由汇编实现
//      9. 汇编中实现的中断处理程序：
//              9.1 压入错误代码
//              9.2 压入上下文：ds, es, fs, gs, eax, ecx, edx, ebx, esp, ebp, esi, edi
//              9.3 压入中断向量号
//              9.4 跳转到C语言的中断处理程序执行
//      10. 执行C语言的中断处理程序
typedef struct __intr_stack {
    // 以下内容是loader.S中实现的中断处理程序，主要是用于保护现场
    uint32_t vec_no;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    // 下面这些内容是中断发生的时候CPU自动填充的
    // 如果目标代码段的访问权限高于当前代码段的权限，则需要保存当前SS、ESP
    // 如果目标代码段的访问权限不高于当前代码段的权限，则直接使用当前的SS、ESP即可，此时不会压入SS和ESP
    // 所以此时最后两个esp和ss的值无关的
    uint32_t err_code;
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void *esp;
    uint32_t ss;
} intr_stack_t;


typedef struct __thread_stack{
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    // 线程初次上CPU前eip指向kernel_thread,即指向线程运行函数
    // 其他时候，CPU指向swith_to的返回地址
    void (*eip)(thread_func *func, void* func_args);

    // 下面的参数只在第一次调度上CPU时候被使用
    void (*ununsed_retaddr);        // 第一次上CPU，调用的是kernel_thread这个C语言程序，所以此时栈顶就是返回地址
    thread_func* function;
    void *func_arg;
} thread_stack_t;


// 进程PC/线程TCB本体
typedef struct __task_struct {
    uint32_t *self_kstack;          // 进程PCB/线程TCB的栈指针
    task_status_t status;           // 进程PCB/线程TCB的状态
    char name[16];                  // 进程PCB/线程TCB的名字
    uint8_t priority;               // 进程PCB/线程TCB的优先级
    uint8_t this_ticks;             // 进程PCB/线程TCB每次在CPU上运行的时钟数
    uint32_t total_ticks;           // 进程PCB/线程TCB被创建以来所有运行的时钟数
    // 操作系统中需要维护两个双向链表：ready进程/线程链表，总进程/线程表
    // 由于进程/线程分别由PCB/TCB表示，因此read进程/线程链表和总进程/线程表中存放的应该是PCB/TCB作为结点的PCB/TCB链表
    // 可是list.c中实现的链表的节点类型是list_elem_t，并不是__task_struct
    // 因此，就使用了general_tag、all_list_tag来组成链表，而后使用list.c中的offset和elem2entry来获取PCB/TCB的入口
    list_elem_t general_tag;        // 当前PCB/TCB在ready线程队列中的结点
    list_elem_t all_list_tag;       // 当前PCB/TCB在全部线程队列thread_all_list中的结点
    uint32_t *pgdir;                // 进程自己的页表的虚拟地址，用于区分进程和线程，线程无此项
    uint32_t stack_magic;           // 栈的边界标记，用于检测栈是否溢出，栈指针被初始化到当前页的最后一个字节，而后向上增长
} task_struct_t;


void thread_init(void);
void thread_create(task_struct_t* tcb, thread_func function, void *func_arg);
void thread_block(task_status_t status);
void thread_unblock(task_struct_t *tcb);

task_struct_t *running_thread(void);

void init_thread(task_struct_t *tcb, char *name, int priority);
task_struct_t *thread_start(char *name, int prio, thread_func function, void* func_args);

void schedule(void);
#endif