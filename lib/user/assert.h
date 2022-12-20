#ifndef __LIB_USER_ASSERT_H
#define __LIB_USER_ASSERT_H

#define panic(...) user_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG
   #define assert(CONDITION) ((void)0)
#else
   #define assert(CONDITION)        \
        if (!(CONDITION)) {         \
	        panic(#CONDITION);      \
        }    

#endif

/**
 * @brief user_spin 用于输出文件名、行数、函数以及条件，是用户态下ASSERT断言的实现函数
 * 
 * @param filename 断言所在的文件名
 * @param line 断言的行
 * @param func 断言所在的函数
 * @param condition 断言的条件
 */
void user_spin(char* filename, int line, const char* func, const char* condition);

#endif
