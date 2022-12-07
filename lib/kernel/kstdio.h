#ifndef __LIB_KERNEL_KSTDIO_H
#define __LIB_KERNEL_KSTDIO_H

#include "global.h"

/**
 * @brief kprintf用于内核进行格式化输出
 * 
 * @param format 格式化字符串
 * @param ... 与格式字符串中格式控制字符一一对应的参数
 */
void kprintf(const char* format, ...);


#endif