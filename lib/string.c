#include "string.h"
#include "global.h"
#include "debug.h"

/**
 * @brief memset将dst_起始的size个字节置为value
 * 
 * @param dst_ 起始地址
 * @param value 设置的值
 * @param size 要设置多少个字节
 */
void memset(void *dst_, uint8_t value, uint32_t size){
    ASSERT(dst_ != NULL);
    uint8_t* dst = (uint8_t *)dst_;
    while(size-- > 0){
        *dst++ = value;
    }
}


/**
 * @brief memcpy将src_起始的size个字节复制到dst_
 * 
 * @param dst_ 被复制的内存区域的起始地址
 * @param src_ 复制到的内存区域的起始地址
 * @param size 复制的字节数
 */
void memcpy(void *dst_, const void* src_, uint32_t size){
    ASSERT(dst_ != NULL && src_ != NULL);
    uint8_t* dst = dst_;
    const uint8_t* src = src_;
    while (size-- > 0)
        *dst++ = *src++;
}

/**
 * @brief memcmp用于比较a_和b_开头的size个字节，相等则返回0
 * 
 * @param a_ 要比较的第一个内存区域的地址
 * @param b_ 要比较的第二个内存区域的地址
 * @param size 要比较的字节数
 * @return int 如果a>b，则返回1；a=b，则返回0，a<b，则返回-1
 */
int memcmp(const void *a_, const void *b_, uint32_t size){
    const char* a = a_;
    const char* b = b_;
    ASSERT(a != NULL && b != NULL);
    while (size-- > 0){
        if (*a != *b)
            return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

/**
 * @brief 复制src_指向的以'\0'结尾的字符串到src_中去
 * 
 * @param des_ 将要复制到的内存地址
 * @param src_ 被复制的字符串
 * @return char* det_
 * 
 */
char* strcpy(char *des_, const char *src_){
    ASSERT(des_ != NULL && src_ != NULL);
    char *r = des_;
    while ((*des_++ = *src_++));
    return r;
}

/**
 * @brief 返回以'\0'结尾的字符串的长度
 * 
 * @param str 要获取长度的字符串
 * @return uint32_t 字符串的长度
 */
uint32_t strlen(const char* str){
    ASSERT(str != NULL);
    const char* p = str;
    while(*p++);
    return (p - str - 1);
}


/**
 * @brief strcmp比较两个字符串，若相等则返回0
 * 
 * @param a 要比较的第一个字符串
 * @param b 要比较的第二个字符串
 * @return int8_t 若a=b，则返回0；若a<b，则返回-1；若a>b，则返回1
 */
int8_t strcmp(const char* a, const char* b){
    ASSERT(a != NULL && b != NULL);
    while(*a != 0 && *a == *b)
        a++, b++;
    return *a < *b ? -1 : *a > *b;
}


/**
 * @brief 返回字符串从左往右第一次出现字符ch的地址
 * 
 * @param str 被搜索的字符串
 * @param ch 要搜索的字符
 * @return char* 
 * 
 * @note strchr相当于返回字符ch第一次出现的地址
 */
char* strchr(const char* str, const uint8_t ch){
    ASSERT(str != NULL);
    while (*str != 0){
        if(*str == ch)
            return (char*) str;
        str++;
    }
    return NULL;
}

/**
 * @brief 返回字符串从右往左第一次出现字符ch的地址
 * 
 * @param str 被搜索的字符
 * @param ch 要搜索的字符
 * @return char* 字符从右向左第一次出现的地址
 * 
 * @note strrchr相当于返回字符ch最后一次出现的地址
 */
char* strrchr(const char* str, const uint8_t ch){
    ASSERT(str != NULL);
    const char* last_char = NULL;
    while (*str != 0){
        if (*str == ch)
            last_char = str;
        str++;
    }
    return (char*) last_char;
}


/**
 * @brief 返回字符串str中字符ch出现的次数
 * 
 * @param str 要被搜索的字符串
 * @param ch 要查询的字符
 * @return uint32_t 字符ch出现的次数 
 */
uint32_t strchrs(const char* str, uint8_t ch){
    ASSERT(str != NULL);
    uint32_t ch_cnt = 0;
    const char* p = str;
    while (*p != 0){
        if (*p == ch)
            ch_cnt++;
        p++;
    }
    return ch_cnt;
}