#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H

#include "stdint.h"

// 段选择子的权限，即RPL
#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

// 段选择子TI位，TI=0在GDT中，TI=1在LDT中
#define TI_GDT 0
#define TI_LDT 1

// 内核段选择子，32位保护模式下，每个段选择子16位，一个int
#define SELECTOR_K_CODE     ((1 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA     ((2 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK    SELECTOR_K_DATA
#define SELECTOR_K_VIDEO    ((3 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_GS       SELECTOR_K_VIDEO

// 中断描述符属性
#define IDT_DESC_P          1
#define IDT_DESC_RPL0       0
#define IDT_DESC_RPL3       3
#define IDT_DESC_32_TYPE    0xE                 // 32位中断门
#define IDT_DESC_16_TYPE    0x6                 // 16位中断门，实际上不会调用

// 中断描述符，32位保护模式下，每个中断描述符32位位，一个int
#define IDT_DESC_ATTR_DPL0  ((IDT_DESC_P << 7) + (IDT_DESC_RPL0 << 5) + IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3  ((IDT_DESC_P << 7) + (IDT_DESC_RPL3 << 5) + IDT_DESC_32_TYPE)


// 常用的定义
#define bool int
#define NULL ((void *) 0)
#define true 1
#define false 0


#endif