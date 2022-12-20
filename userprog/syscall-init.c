#include "syscall-init.h"
#include "thread.h"
#include "print.h"
#include "syscall.h"
#include "console.h"
#include "string.h"
#include "fs.h"
#include "fork.h"
#include "memory.h"
#include "exec.h"
#include "wait_exit.h"

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
 * @brief sys_putchar是putchar系统调用的执行函数, 用于输出一个字符
 * 
 * @param char_ascii 需要输出的字符的Ascii码
 */
void sys_putchar(char char_ascii){
    console_put_char(char_ascii);
}


/**
 * @brief sys_clear是clear系统调用的执行函数, 用于清空当前屏幕
 */
void sys_clear(void){
    cls_screen();
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
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    syscall_table[SYS_OPEN] = sys_open;
    syscall_table[SYS_CLOSE] = sys_close;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_LSEEK] = sys_lseek;
    syscall_table[SYS_UNLINK] = sys_unlink;
    syscall_table[SYS_MKDIR] = sys_mkdir;
    syscall_table[SYS_OPENDIR] = sys_opendir;
    syscall_table[SYS_CLOSEDIR] = sys_closedir;
    syscall_table[SYS_READDIR] = sys_readdir;
    syscall_table[SYS_REWINDDIR] = sys_rewinddir;
    syscall_table[SYS_RMDIR] = sys_rmdir;
    syscall_table[SYS_GETCWD] = sys_getcwd;
    syscall_table[SYS_CHDIR] = sys_chdir;
    syscall_table[SYS_STAT] = sys_stat;
    syscall_table[SYS_FORK] = sys_fork;
    syscall_table[SYS_PUTCHAR] = sys_putchar;
    syscall_table[SYS_CLEAR] = sys_clear;
    syscall_table[SYS_PS] = sys_ps;
    syscall_table[SYS_EXECV] = sys_execv;
    syscall_table[SYS_WAIT] = sys_wait;
    syscall_table[SYS_EXIT] = sys_exit;
    put_str("syscall_init done\n");
}