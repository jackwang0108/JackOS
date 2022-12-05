#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H

#include "stdint.h"

/// @brief va_list (variable argument)与可变参数宏va_start, va_arg, va_end配合
typedef char* va_list;

/**
 * @brief itoa (integer to ascii)用于将整形value转为字符串(ascii), 并存入buf_ptr_addr指向的内存中
 * 
 * @param value 要转换的值
 * @param buf_ptr_addr 指向字符数组的指针
 * @param base Ascii Integer的进制, 例如int a = 16在base=16下的值为"F"
 */
void itoa(uint32_t value, char **buf_ptr_addr, uint8_t base);


/**
 * @brief vsprintf用于按照format中的格式, 将ap的内容复制到str中. 例如: vsprintf(str, "I have %d %s", 10, "books"),
 *          得到结果为: str = "I have 10 books", format不变
 * 
 * @details 目前支持的格式控制字符有:
 *          1. %b 2进制输出一个整数
 *          2. %d 10进制输出一个整数
 *          3. %o 8进制输出一个整数
 *          4. %x 16进制输出一个整数
 *          5. %% 输出一个%
 *          6. %c 输出一个字符
 *          7. %s 输出一个字符串
 * 
 * @param str 接受输出的数组
 * @param format 格式字符串, 例如"number = %d, string = %s'
 * @param ap 变长指针
 * @return uint32_t str中的字符数
 */
uint32_t vsprintf(char *str, const char* format, va_list ap);


/**
 * @brief sprintf将格式化后的字符串输出到字符数组buf中
 * 
 * @param buf 将输出的字符数组
 * @param format 含格式控制字符的格式字符串
 * @param ... 与格式字符串中格式控制字符一一对应的参数
 * @return uint32_t 输出到buf字符数组中的字符个数
 */
uint32_t sprintf(char *buf, const char* format, ...);


/**
 * @brief printf用于格式化输出字符串
 * 
 * @param format 含格式控制字符的格式字符串
 * @param ... 与格式字符串中格式控制字符一一对应的参数
 * @return uint32_t 输出的字符串中的字符数
 */
uint32_t printf(const char* format, ...);

#endif