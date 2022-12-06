#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"

typedef enum __SYSCALL_NR_t {
    SYS_GETPID,
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE
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


/**
 * @brief malloc系统调用将从当前进程的堆中申请size个字节的内存
 * @param size 要申请字节数
 * @return void* 若申请成功, 则返回申请得到的内存第一个字节的地址; 失败则返回NULL
 */
void* malloc(uint32_t size);

/**
 * @brief free系统调用用于将ptr执向的内存归还到当前进程的堆中. 注意, 归还的
 *      内存必须是malloc系统调用分配得到的内存
 * @param ptr 指向要归还的内存的指针
 */
void free(void *ptr);

#endif