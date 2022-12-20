#ifndef __USERPROG_EXEC_H
#define __USERPROG_EXEC_H

#include "global.h"
#include "stdint.h"

extern void intr_exit(void);

typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;


/// @brief 32位elf程序头
typedef struct __Elf32_Ehdr{
    unsigned char   e_ident[16];
    Elf32_Half      e_type;
    Elf32_Half      e_machine;
    Elf32_Word      e_version;
    Elf32_Addr      e_entry;
    Elf32_Off       e_phoff;
    Elf32_Off       e_shoff;
    Elf32_Word      e_flags;
    Elf32_Half      e_ehsize;
    Elf32_Half      e_phentsize;
    Elf32_Half      e_phnum;
    Elf32_Half      e_shentsize;
    Elf32_Half      e_shnum;
    Elf32_Half      e_shstrndx;
} Elf32_Ehdr;


/// @brief 32位elf段描述头
typedef struct __Elf32_Phdr{
    Elf32_Word      p_type;
    Elf32_Off       p_offset;
    Elf32_Addr      p_vaddr;
    Elf32_Addr      p_paddr;
    Elf32_Word      p_filesz;
    Elf32_Word      p_memsz;
    Elf32_Word      p_flags;
    Elf32_Word      p_align;
} Elf32_Phdr;


/// @brief 段类型
typedef enum __segment_type {
    PT_NULL,                ///< 忽略
    PT_LOAD,                ///< 可加载程序段
    PT_DYNAMIC,             ///< 动态加载信息
    PT_INTERP,              ///< 动态加载器名称
    PT_NOTE,                ///< 辅助信息
    PT_SHLIB,               ///< 保留
    PT_PHDR                 ///< 程序头表
} segment_type;


/**
 * @brief sys_execv是execv系统调用的实现函数. 用于将path指向的程序加载到内存中, 而后
 *        用该程序替换当前程序
 * 
 * @param path 程序的绝对路径
 * @param argv 参数列表
 * @return int32_t 若运行成功, 则返回0 (其实不会返回); 若运行失败, 则返回-1
 */
int32_t sys_execv(const char* path, const char *argv[]);

#endif