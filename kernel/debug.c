#include "debug.h"
#include "print.h"
#include "interrupt.h"

/**
 * @brief panic_spin 用于输出文件名、行数、函数以及条件，是ASSERT断言的实现函数
 * 
 * @param filename 断言所在的文件名
 * @param line 断言的行
 * @param func 断言所在的函数
 * @param condition 断言的条件
 */
void panic_spin(char *filename, int line, const char* func, const char* condition){
    intr_disable();
    put_str("\n\n\n!!!!!!!!!! ERROR !!!!!!!!!!\n");
    put_str("filename: "); put_str(filename); put_str("\n");
    put_str("line: 0x"); put_int(line); put_str("\n");
    put_str("function: "); put_str((char*)func); put_str("\n");
    put_str("condistion: "); put_str((char*)condition); put_str("\n");
    while (1);
}