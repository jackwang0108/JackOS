// shared types

#ifndef __LIB_TYPES_H
#define __LIB_TYPES_H

#include "list.h"
#include "stdint.h"
#include "global.h"

/* -------------------------------------- thread -------------------------------------- */

/// 进程或者线程的pid
typedef int16_t pid_t;

/* ---------------------------------------- fs ---------------------------------------- */

/// @brief 文件类型
typedef enum __file_type_t {
    FT_UNKNOW,
    FT_REGULAR,
    FT_DIRECTORY
} file_type_t;


/// @brief 文件的读写类型
typedef enum __oflags_t {
    O_RDONLY    =   0b000,              ///< 文件只读
    O_WRONLY    =   0b001,              ///< 文件只写
    O_RDWD      =   0b010,              ///< 文件读写
    O_CREAT     =   0b100               ///< 文件若不存在, 则创建文件
} oflags_t;


/// @brief 文件读写位置偏移量
typedef enum __whence_t {
    /// @brief SEEK_SET 会将文件新的读写位置设置为相对文件开头的offset字节处, 此时要求offset为正值
    SEEK_SET  =  1,
    /// @brief SEEK_CUR 会将文件新的读写位置设置为相对当前位置的offset字节处, 此时要求offset正负均可
    SEEK_CUR,
    /// @brief SEEK_END 会将文件新的读写位置设置为相对文件结尾的offset字节处, 此时要求offset为负值
    SEEK_END
} whence_t;


/// @brief 文件属性结构体
typedef struct __stat_t {
    /// @brief 文件的inode编号
    uint32_t st_ino;
    /// @brief 文件的大小
    uint32_t st_size;
    /// @brief 文件类型
    file_type_t st_filetype;
} stat_t;


/* ---------------------------------------- inode ---------------------------------------- */


typedef struct __inode_t {
    uint32_t i_no;              ///< inode编号
    /// inode和文件一一对应, 因此不同的文件i_size的含义不同:
    //      1. 若此inode表示的文件是目录文件, 则i_size表示该目录下所有目录项大小之和
    //      2. 若此inode表示的文件是一般文件, 则i_size表示该文件的大小
    uint32_t i_size;
    uint32_t i_open_cnt;        ///< 该inode被打开的次数
    bool write_deny;            ///< 该inode能否被写入, 因此写inode不能并行, 这里是已经有人在写inode的标识
    uint32_t i_sectors[13];     ///< 存储该inode表示的文件的数据的扇区号, i_sectors[0-11]是直接块, i_sectors[12]是一级间接块

    list_elem_t inode_tag;      ///< inode在打开的inode链表中的标记
} inode_t;



/* ---------------------------------------- dir ---------------------------------------- */

#define MAX_FILE_NAME_LEN               16              ///< 文件或者目录名最大16个字符

/// @brief 目录的数据结构, 此项不会存在于磁盘中, 只会存在于内存中
typedef struct __dir_t {
    inode_t *inode;             ///< 存储目录文件的inode号
    uint32_t dir_pos;           ///< 记录在目录内的偏移, 遍历目录的时候会用到
    uint8_t dir_buf[512];       ///< 目录的数据缓存
} dir_t;


typedef struct __dir_entry_t {
    char filename[MAX_FILE_NAME_LEN];       // 文件或者目录的名称
    uint32_t i_no;                          // 文件或者目录对应的inode编号
    file_type_t f_type;                 // 文件类型
}dir_entry_t;

#endif