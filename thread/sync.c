#include "sync.h"
#include "list.h"
#include "interrupt.h"
#include "debug.h"

/**
 * @brief sema_init用于初始化信号量
 * 
 * @param sema 指向需要初始化的信号量的指针
 * @param value 信号量的值
 */
void sema_init(semaphore_t *sema, uint8_t value){
    sema->value = value;
    list_init(&sema->waiters);
}


/**
 * @brief 信号量的P操作，即取走一个资源
 * 
 * @param sema 指向需要操作的信号量的值
 */
void sema_down(semaphore_t *sema){
    // 关中断，保证sema_down操作的原子性
    intr_status_t old_status = intr_disable();
    // 此时已经没有资源，等待资源
    while (sema->value == 0){
        // 此时当前进程不应该在等待队列中
        ASSERT(!elem_find(&sema->waiters, &running_thread()->general_tag));
        if (elem_find(&sema->waiters, &running_thread()->general_tag))
            PANIC("sema_down: blocked thread has been in waiters list")
        list_append(&sema->waiters, &running_thread()->general_tag);
        // 执行到下面这句话之后，线程就中断运行了，直到调用的线程被unblock
        thread_block(TASK_BLOCKED);
    }
    sema->value--;
    ASSERT(sema->value == 0);
    intr_set_status(old_status);
}


/**
 * @brief 信号量的V操作，即释放一个资源
 * 
 * @param sema 指向需要释放资源的信号量的指针
 */
void sema_up(semaphore_t *sema){
    // 保证对sema操作的原子性，需要关中断
    intr_status_t old_status = intr_disable();
    ASSERT(sema->value == 0);
    if (!list_empty(&sema->waiters)){
        task_struct_t *blocked_thread = elem2entry(task_struct_t, general_tag, list_pop(&sema->waiters));
        // 此时中断还是关着的，所以unblock讲被阻塞的线程放到ready_list中也没关系，因为时钟中断不会被响应
        // 所以一定是可以继续执行到下面的sema->value++的
        thread_unblock(blocked_thread);
    }
    sema->value++;
    ASSERT(sema->value == 1);
    intr_set_status(old_status);
}


/**
 * @brief 初始化互斥锁mutex
 * 
 * @param mutex 指向需要初始化的互斥锁的指针
 */
void mutex_init(mutex_t *mutex){
    mutex->holder = NULL;
    mutex->holder_repeat_nr = 0;
    sema_init(&mutex->semaphore, 1);
}


/**
 * @brief 尝试获得互斥锁mutex
 * 
 * @param mutex 将要获得的互斥锁mutex
 */
void mutex_acquire(mutex_t *mutex){
    if (mutex->holder != running_thread()){
        sema_down(&mutex->semaphore);
        mutex->holder = running_thread();
        // 能从sema_down中出来，就一定有资源
        ASSERT(mutex->holder_repeat_nr == 0);
        mutex->holder_repeat_nr = 1;
    } else {
        mutex->holder_repeat_nr ++;
    }
}


/**
 * @brief 尝试释放锁mutex
 * 
 * @param mutex 将要释放的锁mutex
 */
void mutex_release(mutex_t *mutex){
    // 只有当前正在运行的进程才能释放其所占有的锁
    ASSERT(mutex->holder == running_thread());
    if (mutex->holder_repeat_nr > 1){
        mutex->holder_repeat_nr --;
        return;
    }
    // 能运行到这里，一定是等于1
    ASSERT (mutex->holder_repeat_nr == 1);
    mutex->holder = NULL;
    mutex->holder_repeat_nr = 0;
    sema_up(&mutex->semaphore);
}