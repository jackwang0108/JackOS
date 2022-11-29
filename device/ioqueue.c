#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"


/**
 * @brief ioqueue_init用于初始化一个io队列
 * 
 * @param ioq 指向需要被初始化的io队列
 */
void ioqueue_init(ioqueue_t *ioq){
    mutex_init(&ioq->mutex);
    ioq->head = ioq->tail = 0;
    // 初始生产者和消费者设为空
    ioq->producer = ioq->consumer = NULL;
}


/**
 * @brief ioq_full用于判断输入缓冲队列是否满了
 * 
 * @param ioq 需要判断的输入缓冲队列
 * @return true 缓冲队列满
 * @return false 缓冲队列不满
 */
bool ioq_full(ioqueue_t *ioq){
    ASSERT(intr_get_status() == INTR_OFF);
    return next_pos(ioq->head) == ioq->tail;
}

/**
 * @brief ioq_empty用于判断输出缓冲队列是否为空
 * 
 * @param ioq 需要判断的缓冲队列
 * @return true 缓冲队列空
 * @return false 缓冲队列满
 */
bool ioq_empty(ioqueue_t *ioq){
    ASSERT(intr_get_status() == INTR_OFF);
    return ioq->head == ioq->tail;
}

/**
 * @brief ioq_wait用于让当前正在访问的线程（生产者或消费者）等待，同时返回被等待的线程
 * 
 * @param waiter 被睡眠的线程
 * 
 * @note 传入的waiter必须为NULL，由ioq_wait来填充被阻塞的线程
 */
void ioq_wait(task_struct_t **waiter){
    ASSERT(*waiter == NULL && waiter != NULL);
    *waiter = running_thread();
    thread_block(TASK_BLOCKED);
}

/**
 * @brief ioq_wakeup唤醒一个waiter
 * 
 * @param waiter 被唤醒的线程
 */
void ioq_wakeup(task_struct_t **waiter){
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);
    *waiter = NULL;
}

/**
 * @brief ioq_getchar用于从缓冲区中获取一个字符
 * 
 * @param ioq 需要获取字符的缓冲队列
 * @return char 获得的字符
 * 
 * @note 调用该函数的，就是消费者
 */
char ioq_getchar(ioqueue_t *ioq){
    ASSERT(intr_get_status() == INTR_OFF);

    // 若缓冲队列为空，则将当前线程阻塞（调用该函数的，一定是消费者）
    if (ioq_empty(ioq)){
        // ioq是共享变量，因此需要上锁保护
        mutex_acquire(&ioq->mutex);
        // 类似于信号量的condition操作，阻塞当前线程
        ioq_wait(&ioq->consumer);
        mutex_release(&ioq->mutex);
    }
    // 读取字节
    char byte = ioq->buf[ioq->tail];
    ioq->tail = next_pos(ioq->tail);

    // 类似于信号量的signal操作，唤醒生产者
    if (ioq->producer != NULL)
        ioq_wakeup(&ioq->producer);
    
    return byte;
}

/**
 * @brief ioq_putchar用于向输入缓冲区中写入一个字节
 * 
 * @param ioq 要写入字符的缓冲队列
 * @param byte 要写入的字符
 */
void ioq_putchar(ioqueue_t *ioq, char byte){
    ASSERT(intr_get_status() == INTR_OFF);

    while (ioq_full(ioq)){
        // 上锁
        mutex_acquire(&ioq->mutex);
        // 类似于信号量的condition操作，阻塞当前线程
        ioq_wait(&ioq->producer);
        mutex_release(&ioq->mutex);
    }

    ioq->buf[ioq->head] = byte;
    ioq->head = next_pos(ioq->head);

    // 类似于信号量的 ignal操作，唤醒生产者
    if (ioq->consumer != NULL)
        ioq_wakeup(&ioq->consumer);
}