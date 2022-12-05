#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"

typedef enum __SYSCALL_NR_t {
    SYS_GETPID,
    SYS_WRITE
} SYSCALL_NR_t;


/**
 * @brief getpid返回当前用户进程的PID
 * @return uint32_t 用户进程的PID
 */
uint32_t getpid(void);

/**
 * @brief write系统调用将打印指定的字符串到终端上
 * @param str 指向要输出的以'\0'结尾的字符串
 * @return uint32_t 打印的字符串的长度
 */
uint32_t write(char *str);

#endif