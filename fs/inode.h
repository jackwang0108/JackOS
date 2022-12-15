#ifndef __FS_INODE_H
#define __FS_INODE_H

#include "ide.h"
#include "list.h"
#include "stdint.h"

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


/**
 * @brief inode_sync用于将内存中的inode数据写入到磁盘中partition分区的inode_table中
 * 
 * @param partition 要同步的inode所在的分区
 * @param inode 要写入的inode数据
 * @param io_buf 硬盘io缓冲区
 */
void inode_sync(partition_t *partition, inode_t *inode, void *io_buf);

/**
 * @brief inode_open用于打开partition指向的分区中编号为inode_no的inode. 为了加快文件读取速度, 减少磁盘IO
 *          系统维护了一个全局的open_inode链表, 每次要打开一个inode的时候, 首先在open_inode链表中查找,
 *          如果找到了就直接返回inode, 同时inode打开计数+1. 如果没有找到, 则从磁盘中读取inode到内存中.
 * 
 * @param partition 需要打开的inode所在的分区
 * @param inode_no 需要打开的inode在所在分区的inode_table的index
 * @return inode_t* 指向打开的inode. 注意, inode_open会在内核的堆中分配一个sizeof(inode_t), 而后将磁盘中要读取的inode
 *          的信息写入到在内核堆中分配的数据
 */
inode_t *inode_open(partition_t *partition, uint32_t inode_no);


/**
 * @brief inode_close用于关闭inode指向的inode. 具体来说, 该函数会在open_inode链表
 *      中删除inode指向的inode, 或者引用计数减1
 * 
 * @param inode 要关闭的inode
 */
void inode_close(inode_t *inode);


/**
 * @brief inode_delete用于删除磁盘上编号的为inode_no的inode中的信息. 其实此函数是没有必要的
 *      因为只要内存和磁盘中inode bitmap中清零了, 下次创建文件的时候就会覆盖. 
 *      因此, 该函数是调试时测试用的
 * 
 * @param partition 需要删除的inode所在的分区
 * @param inode_no 需要删除的inode在inode_table中的编号
 * @param io_buf 修改磁盘需要的缓冲区, 由调用者提供
 */
void inode_delete(partition_t *partition, uint32_t inode_no, void *io_buf);



/**
 * @brief inode_release用于删除partition分区中编号为inode的文件占用的所有块
 * 
 * @param partition 需要删除的inode所在的分区
 * @param inode_no 需要删除的inode的编号
 */
void inode_release(partition_t *partition, uint32_t inode_no);


/**
 * @brief inode_init用于初始化new_inode指向的块
 * 
 * @param inode_no new_inode指向的块在inode_table中的index
 * @param new_inode 指向需要初始化的块
 */
void inode_init(uint32_t inode_no, inode_t *new_inode);

#endif
