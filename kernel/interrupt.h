#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H

#include "stdint.h"

// 中断处理程序入口地址
typedef void* intr_handler;                 // C语言中void*是一个指针，即地址，所以是32位，表示入口地址

// 函数声明
void idt_init(void);

// 中断的两种状态
typedef enum __intr_status {
    INTR_OFF,                               // 中断关闭状态
    INTR_ON                                 // 中断打开状态
} intr_status_t;

intr_status_t intr_get_status(void);
intr_status_t intr_set_status(intr_status_t);
intr_status_t intr_enable(void);
intr_status_t intr_disable(void);

void register_handler(uint8_t vector_no, intr_handler function);

#endif