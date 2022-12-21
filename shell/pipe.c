#include "fs.h"
#include "file.h"
#include "pipe.h"
#include "ioqueue.h"


/**
 * @brief sys_fd_redirect用于将进程的文件描述符表中的old_local_fd替换为new_local_fd
 * 
 * @param old_local_fd 
 * @param new_local_fd 
 */
void sys_fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd){
    task_struct_t *cur = running_thread();
    // 保留文件描述符的直接替换
    if (new_local_fd < 3)
        cur->fd_table[old_local_fd] = new_local_fd;
    else {
        uint32_t new_global_fd = cur->fd_table[new_local_fd];
        cur->fd_table[old_local_fd] = new_global_fd;
    }
}


/**
 * @brief is_pipe用于判断给定的文件描述符所表示的文件是否是管道
 * 
 * @param local_fd 需要判断的文件的文件描述符
 * @return true 是管道
 * @return false 不是管道
 */
bool is_pipe(uint32_t local_fd){
    uint32_t global_fd = fd_local2global(local_fd);
    return file_table[global_fd].fd_flag == PIPE_FLAG;
}


/**
 * @brief sys_pipe式pipe系统调用的实现函数. 用于创建管道, 创建成功后管道将放在pipefd中
 * 
 * @param pipefd 管道两端的两个文件描述符将被安装到pipefd中
 * @return int32_t  若创建成功则返回0; 若创建失败则返回-1
 */
int32_t sys_pipe(int32_t pipefd[2]){
    int32_t global_fd = get_free_slot_in_global();

    // 申请一个内核页作为环形缓冲区
    file_table[global_fd].fd_inode = get_kernel_pages(1);
    if (file_table[global_fd].fd_inode == NULL)
        return -1;

    // 初始化循环队列
    ioqueue_init((ioqueue_t *)file_table[global_fd].fd_inode);

    // 设置管道标志
    file_table[global_fd].fd_flag = PIPE_FLAG;          ///< fd_flag复用为管道标志
    file_table[global_fd].fd_pos = 2;                   ///< fd_pos复用为管道打开数
    pipefd[0] = pcb_fd_install(global_fd);
    pipefd[1] = pcb_fd_install(global_fd);
    return 0;
}


/**
 * @brief pipe_read用于从fd指定的管道中读取size个字节的数据并存入buf中
 * 
 * @param fd 需要读取的管道的文件描述符
 * @param buf 读取得到的数据将保存到buf中
 * @param count 将读取的字节数
 * @return uint32_t 读取成功的字节数
 */
uint32_t pipe_read(int32_t fd, void *buf, uint32_t count){
    char *bufffer = buf;
    uint32_t bytes_read = 0;
    uint32_t global_fd = fd_local2global(fd);
    ioqueue_t *ioq = (ioqueue_t*)file_table[global_fd].fd_inode;

    uint32_t ioq_len = ioq_length(ioq);
    uint32_t size = ioq_len > count ? count : ioq_len;
    while (bytes_read < size){
        *bufffer = ioq_getchar(ioq);
        bytes_read++;
        bufffer++;
    }
    return bytes_read;
}


/**
 * @brief pipe_write用于将buf中size个字节的数据写入fd表示的管道中
 * 
 * @param fd 需要写入的管道的文件描述符
 * @param buf 将写入的数据的内存缓冲区
 * @param count 将写入的字节数
 * @return uint32_t 成功写入的字节数
 */
uint32_t pipe_write(int32_t fd, const void *buf, uint32_t count){
    uint32_t bytes_write = 0;
    uint32_t global_fd = fd_local2global(fd);
    ioqueue_t *ioq = (ioqueue_t *)file_table[global_fd].fd_inode;

    uint32_t ioq_left = bufsize - ioq_length(ioq);
    uint32_t size = ioq_left > count ? count : ioq_left;
    
    const char *buffer = buf;
    while (bytes_write < size){
        ioq_putchar(ioq, *buffer);
        bytes_write++;
        buffer++;
    }
    return bytes_write;
}