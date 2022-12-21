#ifndef __SHELL_PIPE_H
#define __SHELL_PIPE_H

#include "global.h"
#include "stdint.h"

#define PIPE_FLAG 0xFFFF


/**
 * @brief sys_fd_redirect是fd_redirect系统调用的实现函数. 用于将进程的文件描述符表中的old_local_fd替换为new_local_fd
 * 
 * @param old_local_fd 旧文件描述符
 * @param new_local_fd 新文件描述符
 */
void sys_fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd);


/**
 * @brief is_pipe用于判断给定的文件描述符所表示的文件是否是管道
 * 
 * @param local_fd 需要判断的文件的文件描述符
 * @return true 是管道
 * @return false 不是管道
 */
bool is_pipe(uint32_t local_fd);



/**
 * @brief sys_pipe是pipe系统调用的实现函数. 用于创建管道, 创建成功后管道将放在pipefd中
 * 
 * @param pipefd 管道两端的两个文件描述符将被安装到pipefd中
 * @return int32_t  若创建成功则返回0; 若创建失败则返回-1
 */
int32_t sys_pipe(int32_t pipefd[2]);


/**
 * @brief pipe_read用于从fd指定的管道中读取size个字节的数据并存入buf中
 * 
 * @param fd 需要读取的管道的文件描述符
 * @param buf 读取得到的数据将保存到buf中
 * @param count 将读取的字节数
 * @return uint32_t 读取成功的字节数
 */
uint32_t pipe_read(int32_t fd, void *buf, uint32_t count);



/**
 * @brief pipe_write用于将buf中size个字节的数据写入fd表示的管道中
 * 
 * @param fd 需要写入的管道的文件描述符
 * @param buf 将写入的数据的内存缓冲区
 * @param count 将写入的字节数
 * @return uint32_t 成功写入的字节数
 */
uint32_t pipe_write(int32_t fd, const void *buf, uint32_t count);

#endif