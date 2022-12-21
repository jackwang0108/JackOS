#ifndef __DEICE_IOQUEUE_H
#define __DEVIC_IOQUEUE_H

#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64

// 内核未来只有一个输入缓冲队列，因此是一个共享的数据，所以需要上锁保护
typedef struct __ioqueue_t {
    // 互斥锁
    mutex_t mutex;
    // 生产者、消费者
    task_struct_t *producer;
    task_struct_t *consumer;

    // 缓冲区以及头尾指针
    char buf[bufsize];
    int32_t head;
    int32_t tail;
} ioqueue_t;

/**
 * @brief 
 * 
 * @param cur_pos 
 * @return int32_t 
 */
inline static int32_t next_pos(int32_t cur_pos){
    return (cur_pos + 1) % bufsize;
}


void ioqueue_init(ioqueue_t *ioq);
bool ioq_full(ioqueue_t *ioq);
bool ioq_empty(ioqueue_t *ioq);
void ioq_wait(task_struct_t **waiter);
void ioq_wakeup(task_struct_t **waiter);
char ioq_getchar(ioqueue_t *ioq);
void ioq_putchar(ioqueue_t *ioq, char byte);

/**
 * @brief ioq_length用于获取ioq中的数据总数
 * 
 * @param ioq 需要获取数据总数的ioqueue
 * @return uint32_t ioq中的数据总数
 */
uint32_t ioq_length(ioqueue_t *ioq);

#endif