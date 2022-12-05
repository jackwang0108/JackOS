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
 * @brief sys_write是write系统调用的执行函数, 用于将打印指定的字符串到终端上
 * @param str 指向要输出的以'\0'结尾的字符串
 * @return uint32_t 打印的字符串的长度
 */
uint32_t sys_write(char *str){
    console_put_str(str);
    return strlen(str);
}


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
    put_str("syscall_init done\n");
}