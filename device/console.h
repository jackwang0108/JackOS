#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H

#include "stdint.h"

/**
 * @brief 初始化终端
 */
void console_init(void);


/**
 * @brief 获得终端
 */
void console_acquire(void);


/**
 * @brief 释放终端
 */
void console_release(void);


/**
 * @brief 用于向终端上输出一个字符串
 * 
 * @param str 要输出的字符
 */
void console_put_str(char *str);


/**
 * @brief 用于向终端上输出一个字符
 * 
 * @param char_ascii 要输出的字符
 */
void console_put_char(uint8_t char_ascii);


/**
 * @brief 用于以16进制的方式向终端上输出一个整数
 * 
 * @param num 需要输出的整数
 */
void console_put_int(uint32_t num);

#endif