#ifndef __USERPROG_SYSCALL_INIT_H
#define __USERPROG_SYSCALL_INIT_H

#include "stdint.h"

/**
 * @brief sys_getpid用于返回当前正在运行的进程的PID号
 * 
 * @return uint32_t 当前进程的PID号
 */
uint32_t  sys_getpid(void);

/**
 * @brief sys_write是write系统调用的执行函数, 用于将打印指定的字符串到终端上
 * @param str 指向要输出的以'\0'结尾的字符串
 * @return uint32_t 打印的字符串的长度
 * 
 * @deprecated 该函数已经在fs/fs.c中有更加强大的实现
 */
// uint32_t sys_write(char *str);


/**
 * @brief syscall_init 用于初始化系统调用
 */
void syscall_init(void);


/**
 * @brief sys_putchar是putchar系统调用的执行函数, 用于输出一个字符
 * 
 * @param char_ascii 需要输出的字符的Ascii码
 */
void sys_putchar(char char_ascii);

/**
 * @brief sys_clear是clear系统调用的执行函数, 用于清空当前屏幕
 */
void sys_clear(void);


/**
 * @brief sys_help是help系统调用的执行函数, 用于输出系统的帮助信息
 */
void sys_help(void);

#endif