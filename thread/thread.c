#include "thread.h"
#include "stdint.h"
#include "global.h"
#include "memory.h"
#include "string.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"

#define PG_SIZE 4096

// main_thread是内核的主线程，目前为止都是使用的汇编程序jmp到内核直接运行的
// 但是未来我们需要使用PCB/TCB来管理线程和进程，因此内核的主线程也将作为一个PCB/TCB参与管理
// 所以内核要构建的第一个进程就是内核自己，在构建好后，未来直接视为一个正常的线程、进程参与调度即可
task_struct_t *main_thread;
list_t thread_ready_list;                   // 就绪队列
list_t thread_all_list;                     // 所有进程/线程队列
list_elem_t *thread_tag;                    // 用于标记当前正在运行中的线程

// 该函数实际上是一个汇编函数，调用的时候C语言会自动帮我们压栈，汇编函数最后我们要清理栈
extern void switch_to(task_struct_t *cur, task_struct_t *next);


/**
 * @brief runnning_thread用于获得当前正在运行的线程/进程的PCB，即指向线程的所在的虚拟页的指针
 * 
 * @return task_struct_t* 指向当前正在运行的线程/进程的PCB的指针
 */
task_struct_t *running_thread(void){
    uint32_t esp;
    // 获得当前esp栈顶指针的值
    asm volatile (
        "mov %%esp, %0"
        : "=g" (esp)
    );
    // 目前假设一个进程不会超过一个页，所以取当前虚拟地址的高20位，即为当前虚拟页
    // 而当前虚拟页前半部分存储的就是线程的PCB
    return (task_struct_t *) (esp & 0xFFFFF000);
}


/**
 * @brief kernel_thread用于执行function(func_args)
 * 
 * @param function 将要执行的函数
 * @param func_args 将要执行的函数的参数
 */
static void kernel_thread(thread_func* function, void *func_args){
    // 运行新的线程前，要打开中断，避免后面的时钟中断无效
    intr_enable();
    function(func_args);
}

/**
 * @brief thread_create用于初始化TCB中的线程栈开头部分，使得中断后能够运行，将将要执行的函数和参数放到对应的位置上去
 * 
 * @param tcb 指向线程的tcb(task_struct_t)的指针
 * @param function 将要执行的函数function
 * @param func_arg 将要传入函数function的参数
 */
void thread_create(task_struct_t* tcb, thread_func function, void *func_arg){
    // 先预留出中断使用的栈的空间，中断发生后self_kstack将指向正确的位置
    tcb->self_kstack -= sizeof(intr_stack_t);
    // 然后再预留出线程栈空间，线程栈稍后就会填充
    tcb->self_kstack -= sizeof(thread_stack_t);

    // 填充线程栈
    thread_stack_t *kthread_stack = (thread_stack_t *)tcb->self_kstack;
    kthread_stack->eip = kernel_thread;         // 线程第一次被调度将从kernel_thread函数开始执行
    kthread_stack->function = function;         // 填充需要执行的函数
    kthread_stack->func_arg = func_arg;         // 填充需要执行的函数的参数
    // 寄存器清零
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}


/**
 * @brief init_thread用于初始化线程的TCB，使得调度器能够进行调度
 * 
 * @param tc 线程的TCB，要求是指向摸个虚拟页的
 * @param name 线程的名字
 * @param priority 线程的优先级
 */
void init_thread(task_struct_t *tcb, char *name, int priority){
    // 内存清0，防止遗留字节造成错误
    memset(tcb, 0, sizeof(*tcb));
    strcpy(tcb->name, name);

    // 将操作系统的主程序也封装成一个线程，并且就是调度器运行的第一个进程
    if (tcb == main_thread)
        tcb->status = TASK_RUNNING;
    else
        tcb->status = TASK_READY;

    tcb->pgdir = NULL;
    tcb->this_ticks = priority;
    tcb->total_ticks = 0;
    tcb->priority = priority;
    tcb->stack_magic = 0x20010107;
    //  分配内核线程栈的栈顶指针
    tcb->self_kstack = (uint32_t *) ((uint32_t)tcb + PG_SIZE);
}

/**
 * @brief thread_start用于创建一个线程或进程
 * 
 * @param name 线程/进程的名字
 * @param prio 线程/进程的优先级
 * @param function 线程/进程将要运行的函数
 * @param func_args 将要传入线程/进程的函数
 * @return task_struct_t* 指向被创建的线程/进程的tcb/pcb的指针
 * 
 * @note thread_start中申请了内核内存以存储线程/进程的TCB/PCB，因此在线程/进程运行结束的时候需要释放申请到的内核内存
 * @note 此外，默认TCB/PCB占用一个页，该页中前面的几个字节存储了TCB/PCB的数据，后面存储了TCB/PCB的栈的信息
 */
task_struct_t *thread_start(char *name, int prio, thread_func function, void* func_args){
    // 为线程TCB/进程PCB分配内存空间
    task_struct_t* tcb = (task_struct_t*) get_kernel_pages(1);
    init_thread(tcb, name, prio);                       // 初始化线程TCB信息
    thread_create(tcb, function, func_args);            // 初始化线程TCB中的线程栈kstack的开头部分，使得scheduler能够正常调用

    // 确保线程之前不在就绪队列中，而后将线程加入到就绪队列中
    ASSERT(!elem_find(&thread_ready_list, &tcb->general_tag));
    list_append(&thread_ready_list, &tcb->general_tag);

    // 确保线程之前不在全部队列中，而后将线程加入到全部队列中
    ASSERT(!elem_find(&thread_all_list, &tcb->all_list_tag));
    list_append(&thread_all_list, &tcb->all_list_tag);

    return tcb;
}




/**
 * @brief 用于制作主线程的PCB，而后将其插入到PCB队列中
 * 
 * @note 此函数是在内核时调用的，因此running_thread就会返回内核的虚拟页的指针
 */
static void make_main_thread(void){
    // 获得指向内核所在的虚拟页的指针
    main_thread = running_thread();
    // 初始化内核主线程的
    init_thread(main_thread, "main", 31);
    // 正常线程还需要调用create_init初始化TCB的栈，但是内核线程TCB的栈已经初始化了，因此这里就不需要初始化了
    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);
}


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
void thread_block(task_status_t status){
    // 检测status是否合法
    ASSERT(((status == TASK_BLOCKED) || (status == TASK_HANGING) || (status == TASK_WAITING)))

    // CPU、PCB等是临界资源，需要保护起来
    intr_status_t old_status = intr_disable();

    task_struct_t *cur = running_thread();
    cur->status = status;
    schedule();

    intr_set_status(old_status);
}


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
void thread_unblock(task_struct_t *tcb){
    intr_status_t old_status = intr_disable();

    // 检查status是否合法
    ASSERT(((tcb->status == TASK_BLOCKED) || (tcb->status == TASK_HANGING) || (tcb->status == TASK_WAITING)))
    if (tcb->status != TASK_READY){
        // 被换下的进程一定在所有线程队列中，并且不在就绪队列中，否则报错
        ASSERT(elem_find(&thread_all_list, &tcb->all_list_tag));
        if (elem_find(&thread_ready_list, &tcb->general_tag)){
            PANIC("thread_unblock: blocked thread in ready_list!");
        }
        // 不论是中断调用的，还是主动调用的，放到最前面能够减少response time
        list_push(&thread_ready_list, &tcb->general_tag);
        tcb->status = TASK_READY;
    }
    
    intr_set_status(old_status);
}


/**
 * @brief schedule用于进行一次进程调度
 * 
 * @note schedule被调用的地方一个是在timer_interrupt中，timer_interrupt中会检查进程的时间片是否用完了，
 *      如果用完了，那么就会调用schedule，此时由于running_thread()状态还是TASK_RUNNING，因此就会确定一个新的进程
 *      此时是程序被迫放弃CPU，即被抢占
 * 
 * @note schedule被调用的第二个地方是thread_block，thread_block中主动修改进程当前的状态，然后此时进程主动放弃
 */
void schedule(void){
    // 检测中断状态，schedule不能被中断，因此此时中断必须是关闭状态
    ASSERT(intr_get_status() == INTR_OFF);

    task_struct_t *cur = running_thread();
    if(cur->status == TASK_RUNNING){
        // 若该进程时间片用完了，则将其加入到就绪队列队尾，在此之前要保证进程不在就绪队列中
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur->this_ticks = cur->priority;
        cur->status = TASK_READY;
    } else {
        // 留空
    }
    // 从就绪队列中获得一个进程，而后将其放上CPU进行运行
    // 这里只是改变进程的状态，前一个进程的现场保护、把下一个线程保存的现场调入CPU都是switch里面干的活
    ASSERT(!list_empty(&thread_ready_list));                    // 此时就绪队列一定不能为空
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    task_struct_t *next = elem2entry(task_struct_t, general_tag, thread_tag);
    next->status = TASK_RUNNING;
    switch_to(cur, next);                                       // 任务切换
}

void thread_init(void){
    put_str("thread init start\n");
    list_init(&thread_all_list);
    list_init(&thread_ready_list);
    make_main_thread();
    put_str("thread init done\n");
}