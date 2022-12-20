#ifndef __USERPROG_EXIT_H
#define __USERPROG_EXIT_H

#include "types.h"
#include "stdint.h"


/**
 * @brief sys_wait是wait系统调用的实现函数. 用于让父进程等待子进程调用exit退出, 并将子进程的返回值保存到status中
 * 
 * @param status 子进程的退出状态
 * @return pid_t 若等待成功, 则返回子进程的pid; 若等待失败, 则返回-1
 */
pid_t sys_wait(int32_t *status);


/**
 * @brief sys_exit是exit系统调用的实现函数. 用于主动结束调用的进程
 */
void sys_exit(int32_t status);

#endif