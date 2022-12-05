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
 */
uint32_t sys_write(char *str);


/**
 * @brief syscall_init 用于初始化系统调用
 */
void syscall_init(void);


#endif