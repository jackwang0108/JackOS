#include "print.h"
#include "console.h"
#include "sync.h"
#include "stdint.h"
#include "thread.h"

// 终端这个资源只有一个，所以使用互斥锁保护
mutex_t console_lock;

/**
 * @brief 初始化终端
 * 
 */
void console_init(void){
    mutex_init(&console_lock);
}

/**
 * @brief 获得终端
 * 
 */
void console_acquire(void){
    mutex_acquire(&console_lock);
}

/**
 * @brief 释放终端
 * 
 */
void console_release(void){
    mutex_release(&console_lock);
}


/**
 * @brief 用于向终端上输出一个字符
 * 
 * @param char_ascii 要输出的字符
 */
void console_put_char(uint8_t char_ascii){
    console_acquire();
    put_char(char_ascii);
    console_release();
}

/**
 * @brief 用于向终端上输出一个字符串
 * 
 * @param str 要输出的字符
 */
void console_put_str(char *str){
    console_acquire();
    put_str(str);
    console_release();
}

/**
 * @brief 用于以16进制的方式向终端上输出一个整数
 * 
 * @param num 需要输出的整数
 */
void console_put_int(uint32_t num){
    console_acquire();
    put_int(num);
    console_release();
}