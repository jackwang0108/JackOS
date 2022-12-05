#include "syscall.h"

#define _syscall0(NUMBER)({\
    int retval;             \
    asm volatile (          \
        "int $0x80"         \
        : "=a" (retval)     \
        : "a" (NUMBER)      \
        : "memory"          \
    );                      \
    retval;                 \
})


#define _syscall1(NUMBER, ARG1) ({  \
    int retval;                     \
    asm volatile (                  \
        "int $0x80"                 \
        : "=a" (retval)             \
        : "a" (NUMBER), "b" (ARG1)  \
        : "memory"                  \
    );                              \
    retval;                         \
})


#define _syscall2(NUMBER, ARG1, ARG2) ({        \
    int retval;                                 \
    asm volatile (                              \
        "int $0x80"                             \
        : "=a" (retval)                         \
        : "a" (NUMBER), "b" (ARG1), "c" (ARG2)  \
        : "memory"                              \
    );                                          \
    retval;                                     \
})


#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({                  \
    int retval;                                                 \
    asm volatile (                                              \
        "int $0x80"                                             \
        : "=a" (retval)                                         \
        : "a" (NUMBER), "b" (ARG1), "c" (ARG2), "d" (ARG3)      \
        : "memory"                                              \
    );                                                          \
    retval;                                                     \
})


/**
 * @brief getpid系统调用将返回当前用户进程的PID
 * @return uint32_t 用户进程的PID
 */
uint32_t getpid(void){
    return _syscall0(SYS_GETPID);
}

/**
 * @brief write系统调用将打印指定的字符串到终端上
 * @param str 指向要输出的以'\0'结尾的字符串
 * @return uint32_t 打印的字符串的长度
 */
uint32_t write(char *str){
    return _syscall1(SYS_WRITE, str);
}