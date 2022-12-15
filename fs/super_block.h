#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H

#include "global.h"


// 文件系统对磁盘的管理不是按照磁盘的扇区来的, 而是按照块/簇来的, 块的大小是人为设定的, 一个块可以是4KB, 16KB, 32KB
// 一个块/簇由几个连续的扇区组成, 假设一个块是4KB大小, 每个扇区512字节, 那么一个块就由8个扇区组成

// 因此, 按照块/簇来管理磁盘的原则下, 一个32KB的文件就可以被分为8个块(假设每个块4K大小). 块与块之间可以不连续, 但是块内的8个扇区必须连续
// 也正是因为块本质上就是扇区的集合, 因此块其实就可以用块内的第一个扇区的LBA地址, 即利用块的起始扇区的LBA地址来描述块的地址

// 不同的文件系统对文件的块的管理方式不同, 例如:
//      1. FAT文件系统中, 一个文件的块之间是按照链表的方式进行连接的, 即上一个块的最后几个字节指向下一个块的LBA地址
//      2. ext文件系统中, 一个文件的块之间是按照索引表的方式存储的, 即用一个块记录了剩余所有区块的地址, 这个块被称为inode, index node

// 我们实现的文件系统是一个类ext的文件系统, 也是使用索引的方式来进行存储的, 即使用inode的方式来进行存储的

// inode的数量和文件的数量是一一对应的, 在建立文件系统的时候就已经确定了, 同样, 块的数量也是在建立文件系统的时候就已经决定了(每个块多大)
// 因此, 需要来管理inode和块, 具体得管理方式就是使用bitmap

// 而由于一个分区中安装一个文件系统, 所以在我们的文件系统中出了正常保存文件的块, 还需要一些块来保存块bitmap, inode bitmap等信息
// 这个块就是超级块, 超级块也是块, 因此本质上还是扇区的集合, 只不过超级块可能比一般块8个扇区多一些, 可能有16个扇区

// 由于我们的ext文件系统只是最简单的文件系统, 因此一个块就是一个扇区, 超级块占用一个扇区

/// @brief Sturcture of Super Block, will be store at (Partition Start Sector + 1 Sector)
typedef struct __super_block_t {
    uint32_t magic;                 ///< 用于标记文件系统类型, 支持多文件系统的操作系统会利用该位, JackOS的文件系统的类ext的文件系统, magic number设置为0x20010107

    // 分区数量信息
    uint32_t sec_cnt;               ///< 本分区总的扇区数, 在ide中读取的分区表中的partition_tablb_entry_t中就有大小
    uint32_t inode_cnt;             ///< 本分区的inode数量

    // 分区地址信息
    uint32_t partition_lba_base;    ///< 分区起始扇区的LBA地址

    // 块位图
    uint32_t block_bitmap_lba;      ///< 文件系统块位图的起始扇区LBA地址
    uint32_t block_bitmap_sects;    ///< 文件系统块位图占用的扇区数

    // inode节点位图
    uint32_t inode_bitmap_lba;      ///< 文件系统inode位图起始扇区LBA地址
    uint32_t inode_bitmap_sects;    ///< 文件系统inode位图占用的扇区数

    // inode table
    uint32_t inode_table_lba;       ///< 文件系统inode列表
    uint32_t inode_table_sects;     ///< 文件系统inode列表占用的扇区数

    // data sector
    uint32_t data_start_lba;        ///< 数据区开始的第一个扇区号
    uint32_t root_inode_no;         ///< 根目录的inode节点
    uint32_t dir_entry_size;        ///< 目录项大小

    // padding
    uint8_t pad[460];
} __attribute__((packed)) super_block_t;

#endif