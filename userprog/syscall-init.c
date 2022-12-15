#include "syscall-init.h"
#include "thread.h"
#include "print.h"
#include "syscall.h"
#include "console.h"
#include "string.h"

#define syscall_nr 32

typedef void *syscall;

syscall syscall_table[syscall_nr];


/**
 * @brief sys_getpid是getpid系统调用的执行函数, 用于返回当前正在运行的进程的PID号
 * 
 * @return uint32_t 当前进程的PID号
 */
uint32_t  sys_getpid(void){
    return running_thread()->pid;
}


/**
 * @brief sys_write是write系统调用的实现函数. 用于向fd指定的文件中写入buf中count个字节的数据
 * 
 * @param fd 需要写入的文件描述符
 * @param buf 需要写入的数据所在的缓冲区
 * @param count 需要写入的字节数
 * @return int32_t 成功写入的字节数
 */
extern int32_t sys_write(int32_t fd, const void *buf, uint32_t count);


/**
 * @brief sys_malloc是malloc系统调用的实现函数, 用于在当前进程的堆中申请size个字节的内存, 定义在memory.c中
 * 
 * @details 开启虚拟内存以后, 只有真正的分配物理页, 在页表中添加物理页和虚拟页的映射才会接触到物理页,
 *          除此以外所有分配内存, 分配的都是虚拟内存
 * 
 * @param size 要申请的内存字节数
 * @return void* 若分配成功, 则返回申请得到的内存的首地址; 失败则返回NULL
 */
extern void* sys_malloc(uint32_t size);


/**
 * @brief sys_free用于释放sys_malloc分配的内存. 定义在memory.c中
 * 
 * @param ptr 指向由sys_malloc分配的物理内存
 */
extern void sys_free(void* ptr);


/**
 * @brief syscall_init 用于初始化系统调用, 即在syscall_table中注册各个系统调用
 * 
 * @details 目前已经注册的系统调用有:
 *              1. syscall_table[SYS_GETPID] = sys_getpid
 *              2. syscall_table[SYS_WRITE] = sys_write
 */
void syscall_init(void){
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    put_str("syscall_init done\n");
}