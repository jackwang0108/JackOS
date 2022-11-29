#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include "list.h"
#include "stdint.h"
#include "thread.h"

// 信号量
typedef struct __semaphore_t
{
    uint8_t value;                   // 信号量的value
    list_t waiters;                 // 等待次信号量的线程队列
} semaphore_t;


typedef struct __mutex_t {
    task_struct_t *holder;          // 互斥锁的占有者
    semaphore_t semaphore;         // 互斥锁的semaphore实现，value=1表示只有一个资源，此时信号量就是互斥锁
    uint32_t holder_repeat_nr;      // 锁的持有者重复申请的次数
} mutex_t;


void sema_init(semaphore_t *sema, uint8_t value);
void sema_down(semaphore_t *sema);
void sema_up(semaphore_t *sema);

void mutex_init(mutex_t *mutex);
void mutex_acquire(mutex_t *mutex);
void mutex_release(mutex_t *mutex);


#endif