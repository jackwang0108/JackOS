#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H

#include "stdint.h"


// put_char、put_str和put_int三个函数是汇编函数
// 并且没有使用锁来保护屏幕这个共享变量，如果想要使用更安全的输出，使用console.h

void put_char(uint8_t char_ascii);
void put_str(char* str);
void put_int(uint32_t num);

void set_cursor(uint32_t cursor_pos);
#endif