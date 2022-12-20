#ifndef __USERPORG_FORK_H
#define __USERPROG_FORK_H

#include "global.h"
#include "stdint.h"

/**
 * @brief sys_fork是fork系统调用的实现函数. 用于复制出来一个子进程. 子进程的数据和代码和父进程的数据代码一模一样
 * 
 * @return pid_t 父进程返回子进程的pid; 子进程返回0
 */
pid_t sys_fork(void);

#endif