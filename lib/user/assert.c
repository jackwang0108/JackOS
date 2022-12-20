#include "assert.h"
#include "stdio.h"


/**
 * @brief user_spin 用于输出文件名、行数、函数以及条件，是用户态下ASSERT断言的实现函数
 * 
 * @param filename 断言所在的文件名
 * @param line 断言的行
 * @param func 断言所在的函数
 * @param condition 断言的条件
 */
void user_spin(char* filename, int line, const char* func, const char* condition) {
    printf(
        "\n\n\n\n"
        "filename:  %s\n"
        "line:  %d\n"
        "function:  %s\n"
        "condition:  %s\n", filename, line, func, condition
    );
    while(1);
}
