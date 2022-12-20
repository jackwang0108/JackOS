/**
 * @file thread.h
 * @author Shihong Wang
 * @brief thread module of the kernel
 * @version 0.1
 * @date 2022-11-26
 * 
 * @copyright Copyright (c) 2022
 */

#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "stdint.h"
#include "list.h"
#include "bitmap.h"
#include "memory.h"
#include "types.h"

#define TASK_NAME_LEN 16
#define MAX_FILE_OPEN_PER_PROC 8

/// 线程函数的模板
typedef void thread_func(void*);


// 定义在thread.c中
extern list_t thread_ready_list;                   ///< 就绪队列
extern list_t thread_all_list;                     ///< 所有进程/线程队列

typedef enum __task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
} task_status_t;


/// 中断发生时的调用流程：
///      1. 外部中断到8259A
///      2. 8259A通知CPU发生了某个中断
///      3. CPU收到8259A发来的消息，得知发生中断
///      4. CPU发消息到8259A以获得中断的中断号
///      5. 8259A收到CPU的消息，将中断号发送给CPU
///      6. CPU收到中断号，将确认收到消息发送给8259A
///      7. CPU从idt中根据中断号去获取中断门
///      8. 中断门中保存了中断处理程序中断处理程序的地址、中断门权限、目的表代码段权限，这个中断处理程序地址在kernerl.S中，由汇编实现
///      9. 汇编中实现的中断处理程序：
///              9.1 压入错误代码                                                            (CPU完成部分)
///              9.2 压入上下文：ds, es, fs, gs, eax, ecx, edx, ebx, esp, ebp, esi, edi      (用户完成)
///              9.3 压入中断向量号                                                          (用户完成)
///              9.4 跳转到C语言的中断处理程序执行                                              (用户完成)
///      10. 执行C语言的中断处理程序
typedef struct __intr_stack {
    // 保存当前进程部分上下文信息, 以下内容是kernel.S中实现的中断处理程序intr%1entry宏中压入的
    // 还有一部分进程上下文信息, 这部分内容在switch.S中的switch中压入
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

    // 下面这些内容是中断发生的时候CPU自动填充的，并且在中断返回时自动弹出
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
    /// 保存当前进程部分上下文信息->ebp, 这部分内容在switch.S中的switch中压入，还有一部分信息在intr_stack_t中保存，在kernel.S中由intr%1entry压入
    uint32_t ebp;
    /// 保存当前进程部分上下文信息->ebx, 这部分内容在switch.S中的switch中压入，还有一部分信息在intr_stack_t中保存，在kernel.S中由intr%1entry压入
    uint32_t ebx;
    /// 保存当前进程部分上下文信息->edi, 这部分内容在switch.S中的switch中压入，还有一部分信息在intr_stack_t中保存，在kernel.S中由intr%1entry压入
    uint32_t edi;
    /// 保存当前进程部分上下文信息->esi, 这部分内容在switch.S中的switch中压入，还有一部分信息在intr_stack_t中保存，在kernel.S中由intr%1entry压入
    uint32_t esi;
    
    /// 线程初次上CPU前eip指向kernel_thread,即指向线程运行函数。其他时候，CPU指向swith_to的返回地址
    void (*eip)(thread_func *func, void* func_args); 


    
    /// 第一次上CPU，调用的是kernel_thread这个C语言程序，所以此时栈顶就是返回地址
    void (*ununsed_retaddr);        ///< 该参数只在第一次调度上CPU时候被使用
    thread_func* function;
    void *func_arg;
} thread_stack_t;



// 内核线程TCB本体
typedef struct __task_struct {
    /* ------------------------------ 内核线程的基本信息 ------------------------------ */
    /// 内核线程TCB的栈顶指针，初始化时候需要被设置为指向页的页尾
    uint32_t *self_kstack;
    /// 内核线程的PID
    pid_t pid;
    /// 内核线程的父线程的PID
    pid_t parent_pid;
    /// 内核线程TCB的状态
    task_status_t status;
    /// 内核线程TCB的名字
    char name[16];
    /// 内核线程TCB的优先级
    uint8_t time_slice;
    /// 内核线程TCB每次在CPU上运行的时钟数
    uint8_t this_tick;
    /// 内核线程TCB被创建以来所有运行的时钟数
    uint32_t total_ticks;
    /// 内核线程打开的文件描述符列表
    int32_t fd_table[MAX_FILE_OPEN_PER_PROC];

    /* ------------------------------ 内核线程所在的链表 ------------------------------ */
    // 操作系统中需要维护两个双向链表：ready内核线程链表，总内核线程表
    // 由于内核线程由TCB表示，因此ready内核线程链表和总内核线程表中存放的应该是TCB作为结点的TCB链表
    // 可是list.c中实现的链表的节点类型是list_elem_t，并不是__task_struct
    // 因此，就使用了general_tag、all_list_tag来组成链表，而后使用list.c中的offset和elem2entry来获取TCB的入口

    /// 当前PCB/TCB在ready线程队列中的结点
    list_elem_t general_tag;
    /// 当前PCB/TCB在全部线程队列thread_all_list中的结点
    list_elem_t all_list_tag;

    /* ------------------------------ 用户进程内存管理 ------------------------------ */
    /// 进程自己的页表的虚拟地址，用于区分进程和线程，线程无此项
    uint32_t *pgdir;
    /// 用户进程的虚拟地址
    virtual_addr_t userprog_vaddr;
    /// 用户进程不同大小内存单元的售货窗口
    mem_block_desc_t u_block_desc[MEM_UNIT_CNT];


    /* ------------------------------ Miscellaneous ------------------------------ */
    /// 当前进程所在的工作目录的inode编号
    uint32_t cwd_inode_no;
    /// 当前进程运行结束后的返回值
    int8_t exit_status;


    /// 栈的边界标记，用于检测栈是否溢出，栈指针被初始化到当前页的最后一个字节，而后向上增长，即向低地址增长
    uint32_t stack_magic;
} task_struct_t;


/**
 * @brief fork_pid用于为子进程分配PID
 * 
 * @return pid_t 子进程分配得到的PID
 */
pid_t fork_pid(void);


/**
 * @brief release_pid用于释放PID
 * 
 * @param pid 需要释放的PID号
 */
void release_pid(pid_t pid);


/**
 * @brief pid2thread用于返回给定pid所表示的pcb
 * 
 * @param pid 需要的tcb的pid
 * @return task_struct_t* 若查找成功, 则返回该tcb; 若失败, 则返回NULL
 */
task_struct_t *pid2thread(int32_t pid);


/**
 * @brief init_thread用于初始化线程的TCB，使得调度器能够进行调度。该函数完成的事为：

 *          1. TCB所占用的内存区域清零
 *          2. 设置线程状态为RASK_READY, 若为内核线程，则直接设置为TASK_RUNNING
 *          3. 初始化TCB中的数据, 包括pgdir, priority, this_tick, total_ticks, time_slice
 *          4. 注意，TCB中的self_kstack被设置为TCB所在这这一页的页尾
 * 
 * @param tc 线程的TCB，要求是指向摸个虚拟页的
 * @param name 线程的名字
 * @param time_slice 线程的时间片大小，以时钟中断数为单位
 * 
 */
void thread_init(void);


/**
 * @brief thread_create用于初始化tcb指向的内核线程的线程栈。
 * 
 * @details 该函数具体干的事情:
 *          1. 用于运行用户进程的内核线程以中断返回的形式进入到3特权级运行，因此这里为用户进程对应的内核线程分配一个intr_stack_t
 *          2. 为了能够让内核线程第一次能够成功运行，所以还需要分配一个thread_stack_t，里面存储第一次运行时候的函数和传入函数的参数
 * 
 * @param tcb 指向线程的tcb(task_struct_t)的指针
 * @param function 将要执行的函数function
 * @param func_arg 将要传入函数function的参数
 */
void thread_create(task_struct_t* tcb, thread_func function, void *func_arg);


/**
 * @brief thread_exit用于回收tcb指向的tcb和页表, 并将其从调度队列中去除
 * 
 * @param tcb 需要回收的tcb
 * @param need_schedule 回收tcb后是否需要立即进行调度
 */
void thread_exit(task_struct_t *tcb, bool need_schedule);


/**
 * @brief 将当前进程从CPU上换下，并且设置其状态为status
 * 
 * @param status 当前进程将被设置的状态
 * 
 * @note thread_block的原理就是当前正在运行的进程running_thread在运行的时候一定是在schedule中被pop出来的
 *      因此running_thread此时一定不在ready_list中，只在all_list中。所以只需要将其状态改变，而后进行调度即可
 *      此时被阻塞的进程就被保存在all_list中，而不在ready_list中，因此调用schedule之后，就会有新的进程运行
 * 
 * @note 系统不负责维护被阻塞线程的队列，也不负责唤醒被阻塞的线程。维护被阻塞线程的队列由调用者来维护，并且由调用者决定
 *      何时唤醒被阻塞的线程
 */
void thread_block(task_status_t status);

/**
 * @brief thread_unblock用于将一个被阻塞的线程放入ready_list中
 * 
 * @param tcb 指向要被unblock的线程
 * 
 * @note thread_unblock的原理就是改变进程的状态，而后将其加入到ready_list中即可，并没有发生真正的调度
 *
 * @note 系统不负责维护被阻塞线程的队列，也不负责唤醒被阻塞的线程。维护被阻塞线程的队列由调用者来维护，并且由调用者决定
 *      何时唤醒被阻塞的线程
 */
void thread_unblock(task_struct_t *tcb);


/**
 * @brief thread_yield用于放弃CPU, 和thread_block中线程被动放弃CPU不同, thread_yield表示线程主动放弃CPU
 *        因此线程的状态被设置为TRHEAD_READY, 而非THREAD_BLOCKED
 */
void thread_yield(void);


/**
 * @brief thread_start用于创建一个内核线程, 并且将内核线程加入到就绪队列中，等待运行
 * 
 * @param name 内核线程的名字
 * @param time_slice 内核线程的时间片
 * @param function 内核线程将要运行的函数
 * @param func_args 将要传入内核线程将执行的函数
 * @return task_struct_t* 指向被创建的内核线程的指针
 * 
 * @note thread_start中申请了内核内存以存储线程/进程的TCB/PCB，因此在线程/进程运行结束的时候需要释放申请到的内核内存
 * @note 此外，默认TCB/PCB占用一个页，该页中前面的几个字节存储了TCB/PCB的数据，后面存储了TCB/PCB的栈的信息
 */
task_struct_t *thread_start(char *name, int time_slice, thread_func function, void* func_args);


/**
 * @brief runnning_thread用于获得指向当前正在运行的内核线程的TCB的指针，即指向内核线程的所在的虚拟页的指针
 * 
 * @return task_struct_t* 指向当前正在运行的内核线程的TCB的指针
 */
task_struct_t *running_thread(void);

/**
 * @brief init_thread用于初始化线程的TCB，使得调度器能够进行调度。
 * 
 * 
 * @details 该函数完成的事情:
 *          1. TCB所占用的内存区域清零
 *          2. 设置线程状态为RASK_READY, 若为内核线程，则直接设置为TASK_RUNNING
 *          3. 初始化TCB中的数据, 包括pgdir, priority, this_tick, total_ticks, time_slice
 *          4. 注意，TCB中的self_kstack被设置为TCB所在这这一页的页尾
 * 
 * @param tc 线程的TCB，要求是指向摸个虚拟页的
 * @param name 线程的名字
 * @param time_slice 线程的时间片大小，以时钟中断数为单位
 */
void init_thread(task_struct_t *tcb, char *name, int time_slice);



/**
 * @brief schedule用于进行一次进程调度
 * 
 * @note schedule被调用的地方一个是在timer_interrupt中，timer_interrupt中会检查进程的时间片是否用完了，
 *      如果用完了，那么就会调用schedule，此时由于running_thread()状态还是TASK_RUNNING，因此就会确定一个新的进程
 *      此时是程序被迫放弃CPU，即被抢占
 * 
 * @note schedule被调用的第二个地方是thread_block，thread_block中主动修改进程当前的状态，然后此时进程主动放弃
 */
void schedule(void);


/**
 * @brief sys_ps是ps系统调用的实现函数. 用于遍历thread_all_list, 输出进程信息
 * 
 */
void sys_ps(void);


/**
 * @brief init是第一个用户进程
 */
void init(void);
#endif