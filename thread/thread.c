#include "thread.h"
#include "global.h"
#include "string.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "process.h"
#include "console.h"
#include "sync.h"


/// @brief next_pid是全局的pid线程池, 每次创建新进程的时候都会+1. 由于是全局共享的数据, 因此需要使用锁来保护
mutex_t next_pid_lock;                              ///< next_pid的锁

/**
 * @brief main_thread是内核的主线程，目前为止都是使用的汇编程序jmp到内核直接运行的
 *      但是未来我们需要使用PCB/TCB来管理线程和进程，因此内核的主线程也将作为一个PCB/TCB参与管理
 *      所以内核要构建的第一个进程就是内核自己，在构建好后，未来直接视为一个正常的线程、进程参与调度即可
 */
task_struct_t *main_thread;

list_t thread_ready_list;                   // 就绪队列
list_t thread_all_list;                     // 所有进程/线程队列
list_elem_t *thread_tag;                    // 用于标记当前正在运行中的线程

// 该函数实际上是一个汇编函数，调用的时候C语言会自动帮我们压栈，汇编函数最后我们要清理栈
extern void switch_to(task_struct_t *cur, task_struct_t *next);


/**
 * @brief allocate_pid用于为内核线程分配PID
 * 
 * @return pid_t 线程分配得到的PID
 */
static pid_t allocate_pid(void){
    static pid_t next_pid = 0;
    mutex_acquire(&next_pid_lock);
    next_pid++;
    mutex_release(&next_pid_lock);
    return next_pid;
}


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
 * @brief kernel_thread用于执行function(func_args).
 * 
 * @details kenerl_thread函数其实就是当前线程开始运行时候的启动函数.
 *
 *      当内核线程(next)第一次被调度上CPU的时候, 线程的页表以及switch_to函数干的事如下:
 *          1. 首先修改esp为self_kstack, 这一步由mov eax, [esp + 24]; mov esp, eax完成
 *          2. 而后四个pop把ebp, ebx, edi, esi从栈中弹出来, 此时self_kstack指向了eip (4007处)
 *          3. 接下来ret, 此时CPU自动pop EIP, 而因为是使用的ret来执行函数kernel_thread的, 所以CPU以为之前运行了call switch_to, 所以CPU
 *                           Offset:   |--------------------------------------------------------|
 *                                0    | uint32_t     *tcb.self_kstack                          | ------------------------
 *                                     |--------------------------------------------------------|                        |
 *                              ...    |                          ......                        |                        |
 *                                     |--------------------------------------------------------|                        |
 *  thread_create中创建         3987    | uint32_t     ebp                                       |    thread_stack_t <-----
 *                                     |--------------------------------------------------------|
 *                              ...    |                          ......                        |
 *                                     |--------------------------------------------------------|
 *                             4007    | void         (*eip)(thread_func *func, void* func_arg) | ----> kernel_thread的虚拟地址, 编译的时候自动转换
 *                                     |--------------------------------------------------------|
 *                             4011    | void         *unused_retadd                            |   这里的占位是因为, C语言编译出来的函数调用都是call的形式调用的, 所以C语言回去esp+4找第一个参数, 因此要有一个占位的
 *                                     |--------------------------------------------------------|
 *                             4015    | thread_func* function                                  | ----> function
 *                                     |--------------------------------------------------------|
 *                             4015    | void         *func_arg                                 | ----> func_arg
 *                                     |--------------------------------------------------------|
 *  thread_create中创建         4019    | uint32_t     vec_no                                    |    intr_stack_t
 *                                     |--------------------------------------------------------|
 *                              ...    |                          ......                        |
 *                                     |--------------------------------------------------------|
 *                             4071    | uint32_t     err_code                                  |
 *                                     |--------------------------------------------------------|
 *                             4075    | void         (*eip)(void)                              |
 *                                     |--------------------------------------------------------|
 *                             4079    | uint32_t     cs                                        |
 *                                     |--------------------------------------------------------|
 *                             4083    | uint32_t     eflags                                    |
 *                                     |--------------------------------------------------------|
 *                             4087    | void         *esp                                      |
 *                                     |--------------------------------------------------------|
 *                             4091    | uint32_t     ss                                        |
 *                                     |--------------------------------------------------------|
 * 
 * @param function 将要执行的函数
 * @param func_args 将要执行的函数的参数
 */
static void kernel_thread(thread_func* function, void *func_args){
    // 运行新的线程前，要打开中断，避免后面的时钟中断无效
    intr_enable();
    // console_put_str("In kernerl_thread, ");
    // console_put_str("function addr: 0x"), console_put_int((uint32_t)function), console_put_str("1234567890");
    // console_put_char('\n');
    // console_put_str("func_arg addr: 0x"), console_put_int((uint32_t)func_args), console_put_char('\n');
    function(func_args);
}

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
void thread_create(task_struct_t* tcb, thread_func function, void *func_arg){
    // 下面这intr_stack_t是给初始化用户进程对应的内核线程准备的
    // 中断发生的时候，使用的intr_stack_t和thread_stack_t现场建即可
    tcb->self_kstack -= sizeof(intr_stack_t);               // 先预留出进程初始化使用的栈的空间
    // 而thread_stack_t内核线程和用户线程初始化都用得到
    tcb->self_kstack -= sizeof(thread_stack_t);             // 然后再预留出线程栈空间，线程栈稍后就会填充

    // 填充线程栈
    thread_stack_t *kthread_stack = (thread_stack_t *)tcb->self_kstack;
    kthread_stack->eip = kernel_thread;         // 线程第一次被调度将从kernel_thread函数开始执行
    kthread_stack->function = function;         // 填充需要执行的函数
    kthread_stack->func_arg = func_arg;         // 填充需要执行的函数的参数
    // 寄存器清零
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}


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
void init_thread(task_struct_t *tcb, char *name, int time_slice){
    // 内存清0，防止遗留字节造成错误
    memset(tcb, 0, sizeof(*tcb));
    // 分配PID
    tcb->pid = allocate_pid();
    strcpy(tcb->name, name);

    // 操作系统的主程序也被封装成一个线程，并且就是调度器运行的第一个进程
    // 因此如果初始化的是内核主线程，需要直接将其设置为RUNNING
    if (tcb == main_thread)
        tcb->status = TASK_RUNNING;
    else
        tcb->status = TASK_READY;

    tcb->pgdir = NULL;
    tcb->this_tick = time_slice;
    tcb->total_ticks = 0;
    tcb->time_slice = time_slice;
    tcb->stack_magic = 0x20010107;
    //  分配内核线程栈的栈顶指针
    tcb->self_kstack = (uint32_t *) ((uint32_t)tcb + PG_SIZE);
}

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
task_struct_t *thread_start(char *name, int time_slice, thread_func function, void* func_args){
    // 为线程TCB/进程PCB分配内存空间
    task_struct_t* tcb = (task_struct_t*) get_kernel_pages(1);
    init_thread(tcb, name, time_slice);                     // 初始化线程TCB信息
    thread_create(tcb, function, func_args);                // 初始化线程TCB中的线程栈kstack的开头部分，使得scheduler能够正常调用

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
        ASSERT(!elem_find(&thread_ready_list, &tcb->general_tag));
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
        cur->this_tick = cur->time_slice;
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
    process_activate(next);
    switch_to(cur, next);                                       // 任务切换
}

void thread_init(void){
    put_str("thread init start\n");
    list_init(&thread_all_list);
    list_init(&thread_ready_list);
    mutex_init(&next_pid_lock);
    make_main_thread();
    put_str("thread init done\n");
}