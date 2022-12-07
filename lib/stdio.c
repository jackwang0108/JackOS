#include "global.h"
#include "string.h"
#include "stdio.h"
#include "syscall.h"



/**
 * @brief itoa (integer to ascii)用于将整形value转为字符串(ascii), 并存入buf_ptr_addr指向的内存中
 * 
 * @param value 要转换的值
 * @param buf_ptr_addr 指向字符数组的指针
 * @param base Ascii Integer的进制, 例如int a = 16在base=16下的值为"F"
 */
void itoa(uint32_t value, char **buf_ptr_addr, uint8_t base){
    uint32_t m = value % base;
    uint32_t i = value / base;
    if (i)
        itoa(i, buf_ptr_addr, base);        // 从最高位开始转换, 因为一个数字0x1234, 转成字符串后是'1234', 所以高位要放在低位
    if (m < 10)
        *((*buf_ptr_addr)++) = m + '0';     // ++表示在计算的最后加
    else
        *((*buf_ptr_addr)++) = m - 10 + 'A';
}

/**
 * @brief vsprintf用于按照format中的格式, 将ap的内容复制到str中. 例如: vsprintf(str, "I have %d %s", 10, "books"),
 *          得到结果为: str = "I have 10 books", format不变
 * 
 * @details 目前支持的格式控制字符有:
 *          1. %b 2进制输出一个整数
 *          2. %d 10进制输出一个整数
 *          3. %o 8进制输出一个整数
 *          4. %x 16进制输出一个整数
 *          5. %% 输出一个%
 *          6. %c 输出一个字符
 *          7. %s 输出一个字符串
 * 
 * @param str 接受输出的数组
 * @param format 格式字符串, 例如"number = %d, string = %s'
 * @param ap 变长指针
 * @return uint32_t str中的字符数
 */
uint32_t vsprintf(char *str, const char* format, va_list ap){
    char *buf_ptr = str;
    const char *index_ptr = format;
    char index_char = *index_ptr;

    int32_t arg_int;
    char *arg_str;
    while (index_char){
        // 不是%, 则将字符复制到str中, 而后继续扫描
        if (index_char != '%'){
            *(buf_ptr++) = index_char;
            index_char = *(++index_ptr);
            continue;
        }
        // 获%后面具体的控制字符
        index_char = *(++index_ptr);
        switch (index_char){
            // 输出字符串
            case 's':
                arg_str = va_arg(ap, char*);
                strcpy(buf_ptr, arg_str);
                buf_ptr += strlen(arg_str);
                index_char = *(++index_ptr);
                break;
            // 输出单个字符
            case 'c':
                *(buf_ptr++) = va_arg(ap, char);
                index_char = *(++index_ptr);
                break;
            // 输出二进制的整数
            case 'b':
                arg_int = va_arg(ap, int);
                itoa(arg_int, &buf_ptr, 2);
                index_char = *(++index_ptr);
                break;
            // 输出二进制的整数
            case 'o':
                arg_int = va_arg(ap, int);
                itoa(arg_int, &buf_ptr, 8);
                index_char = *(++index_ptr);
                break;
            // 输出二进制的整数
            case 'd':
                arg_int = va_arg(ap, int);
                if (arg_int < 0){
                    arg_int = 0 - arg_int;
                    *buf_ptr++ = '-';
                }
                itoa(arg_int, &buf_ptr, 10);
                index_char = *(++index_ptr);
                break;
            // 十六进制输出整数
            case 'x':
                arg_int = va_arg(ap, int);      // 将ap指向的参数转为int
                itoa(arg_int, &buf_ptr, 16);
                index_char = *(++index_ptr);
                break;
            // 单独输出一个%
            case '%':
                *(buf_ptr++) = '%';
                index_char = *(++index_ptr);
            default:
                break;
        }
    }
    return strlen(str);
}


/**
 * @brief sprintf将格式化后的字符串输出到字符数组buf中
 * 
 * @param buf 将输出的字符数组
 * @param format 含格式控制字符的格式字符串
 * @param ... 与格式字符串中格式控制字符一一对应的参数
 * @return uint32_t 输出到buf字符数组中的字符个数
 */
uint32_t sprintf(char *buf, const char* format, ...){
    va_list args;
    uint32_t retval;
    va_start(args, format);
    retval = vsprintf(buf, format, args);
    va_end(args);
    return retval;
}


/**
 * @brief printf用于格式化输出字符串
 * 
 * @param format 含格式控制字符的格式字符串
 * @param ... 与格式字符串中格式控制字符一一对应的参数
 * @return uint32_t 输出的字符串中的字符数
 */
uint32_t printf(const char* format, ...){
    va_list args;
    va_start(args, format);
    char buf[1024] = {0};
    vsprintf(buf, format, args);
    va_end(args);
    return write(buf);
}