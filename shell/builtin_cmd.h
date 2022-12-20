#ifndef __SHELL_BUILTIN_CMD_H
#define __SHELL_BUILTIN_CMD_H


#include "stdint.h"


/**
 * @brief make_clear_abs_path用于将包含相对路径的目录path('.'和'..')处理不含相对路径的目录, 并存入final_path中
 * 
 * @param path 包含相对路径的目录
 * @param final_path 不包含相对路径的目录
 */
void make_clear_abs_path(char *path, char *final_path);


/**
 * @brief builtin_pwd是pwd内建命令的实现函数
 * 
 * @param argc 参数个数, 用不到, 统一格式用
 * @param argv 参数个数, 用不到, 统一格式用
 */
void builtin_pwd(uint32_t argc, char **argv __attribute__((unused)));


/**
 * @brief buildin_cd是cd内建命令的实现函数
 * 
 * @param argc 参数个数, 用不到, 统一格式用
 * @param argv 参数个数, 用不到, 统一格式用
 */
char *buildin_cd(uint32_t argc, char **argv);


/**
 * @brief buildin_cd是cd内建命令的实现函数
 * 
 * @param argc 参数个数, cd命令支持一个参数
 * @param argv 参数个数, cd命令的参数要求是目标目录
 */
char *buildin_cd(uint32_t argc, char **argv);


/**
 * @brief builin_ls是ls内置命令的实现函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 */
void builtin_ls(uint32_t argc, char **argv);


/**
 * @brief builtin_ps是ps内置命令的实现函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 */
void builtin_ps(uint32_t argc, char **argv);


/**
 * @brief builtin_clear是clear系统调用的实现函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 */
void builtin_clear(uint32_t argc, char **argv);


/**
 * @brief builtin_mkdir是mkdir命令的内建函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 * @return int32_t 若创建成功, 则返回0; 若创建失败, 则返回-1
 */
int32_t builtin_mkdir(uint32_t argc, char **argv);


/**
 * @brief builin_rmdir是rmdir的内建函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t builtin_rmdir(uint32_t argc, char **argv);


/**
 * @brief builtin_rm是rm内置命令的实现函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t builtin_rm(uint32_t argc, char **argv);



int32_t builtin_touch(uint32_t argc, char **argv);

#endif