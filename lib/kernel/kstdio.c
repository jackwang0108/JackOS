#include "stdio.h"
#include "console.h"
#include "kstdio.h"


/**
 * @brief kprintf用于内核进行格式化输出
 * 
 * @param format 格式化字符串
 * @param ... 与格式字符串中格式控制字符一一对应的参数
 */
void kprintf(const char* format, ...){
    va_list args;
    va_start(args, format);
    char buf[1024] = {0};
    vsprintf(buf, format, args);
    va_end(args);
    console_put_str(buf);
}