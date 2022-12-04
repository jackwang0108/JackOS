#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H

// 中断发生时的调用流程：
//      1. 外部中断到8259A
//      2. 8259A通知CPU发生了某个中断
//      3. CPU收到8259A发来的消息，得知发生中断
//      4. CPU发消息到8259A以获得中断的中断号
//      5. 8259A收到CPU的消息，将中断号发送给CPU
//      6. CPU收到中断号，将确认收到消息发送给8259A
//      7. CPU从idt中根据中断号去获取中断门
//              关于IDT 1: GDT中存放的段描述符指向普通的一段内存，而IDT中的存放的段描述符指向特殊的一段内存
//              关于IDT 2: 这片特殊的内存区域存储的数据和一般段中存储的程序、程序中的数据不同，这些特殊的内存区域存储的数据是专门用于处理中断的
//              关于IDT 3: 所以IDT就像是GDT的一个翻板，已有一个idtr保存idt的位置
//              关于IDT 4: 此外，中断描述符表中存储的段描述符，称为门。根据提具体的作用的不同，分为中断门、陷阱门、任务门
//      8. 中断门中保存了中断处理程序中断处理程序的地址，中断门权限，目的表代码段权限，这个中断处理程序地址在kernerl.S中，由汇编实现
//      9. 汇编中实现的中断处理程序：
//              9.1 压入错误代码
//              9.2 压入上下文：ds, es, fs, gs, eax, ecx, edx, ebx, esp, ebp, esi, edi
//              9.3 压入中断向量号
//              9.4 跳转到C语言的中断处理程序执行
//      10. 执行C语言的中断处理程序

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