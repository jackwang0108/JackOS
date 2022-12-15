#include "fs.h"
#include "file.h"
#include "inode.h"
#include "debug.h"
#include "string.h"
#include "interrupt.h"

typedef struct __inode_position_t {
    bool multi_sec;             /// inode是否跨扇区
    uint32_t sec_lba;           /// inode所在的扇区lba地址
    uint32_t off_size;          /// inode在扇区中的偏移量
} inode_position_t;


/**
 * @brief inode_locate用于计算给定的inode的位置信息
 * 
 * @param partition 要计算的inode所在的分区
 * @param inode_no 要计算的inode在inode_table中的偏移
 * @param inode_position 计算后的结果将写入到inode_position中
 */
static void inode_locate(partition_t *partition, uint32_t inode_no, inode_position_t *inode_position){
    // 一个分区最多4096个文件
    ASSERT(inode_no < 4096);

    // inode_table的lba扇区号
    uint32_t inode_table_lba = partition->sb->inode_table_lba;

    uint32_t inode_size = sizeof(inode_t);
    // inode相对于inode_table的字节offset
    uint32_t inode_table_offset = inode_size * inode_no;
    // inode相对于inode_table的扇区offset
    uint32_t inode_sec_offset = inode_table_offset / 512;
    // inode相对于所在section的字节offset
    uint32_t offset_in_sec = inode_table_offset % 512;

    // 判断inode是否使用了两个扇区来存储
    uint32_t left_in_sec = 512 - offset_in_sec;
    // 如果扇区剩余的空间比inode占用的字节小, 那么肯定占用了两个扇区
    if (left_in_sec < inode_size)
        inode_position->multi_sec = false;
    else
        inode_position->multi_sec = true;
    
    // 写入inode_position信息
    inode_position->sec_lba = inode_table_lba + inode_sec_offset;
    inode_position->off_size = offset_in_sec;
}


/**
 * @brief inode_sync用于将内存中的inode数据写入到磁盘中partition分区的inode_table中
 * 
 * @param partition 要同步的inode所在的分区
 * @param inode 要写入的inode数据
 * @param io_buf 硬盘io缓冲区
 */
void inode_sync(partition_t *partition, inode_t *inode, void *io_buf){
    uint8_t inode_no = inode->i_no;

    // 定位inode
    inode_position_t inode_pos;
    inode_locate(partition, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba <= (partition->start_lba + partition->sec_cnt));

    // 复制inode中的数据到new_inode中, 稍后操作这个inode即可
    inode_t new_inode;
    memcpy(&new_inode, inode, sizeof(inode_t));

    // 清除inode中的一些多余信息, 这些信息是在内存中的inode才会用到的
    new_inode.i_open_cnt = 0;
    new_inode.write_deny = false;
    new_inode.inode_tag.prev = new_inode.inode_tag.next = NULL;

    
    int sector_to_manipulate = 0;
    char *inode_buf = (char*) io_buf;
    if (inode_pos.multi_sec)
        sector_to_manipulate = 2;       // 如果要写入的inode跨扇区, 则要读出 & 写入两个扇区,
    else 
        sector_to_manipulate = 1;
    // 读一个/两个扇区的数据, 然后写入一个/两个扇区
    ide_read(partition->my_disk, inode_pos.sec_lba, inode_buf, sector_to_manipulate);
    memcpy(inode_buf + inode_pos.off_size, &new_inode, sizeof(inode_t));
    ide_write(partition->my_disk, inode_pos.sec_lba, inode_buf, sector_to_manipulate);
}



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
inode_t *inode_open(partition_t *partition, uint32_t inode_no){

    // 先遍历打开的inode链表, 查看是否要打开的inode是否存在
    // 如果存在则直接返回
    inode_t *inode_found;
    list_elem_t *elem = partition->open_inodes.head.next;
    while (elem != &partition->open_inodes.tail){
        inode_found = elem2entry(inode_t, inode_tag, elem);
        if (inode_found->i_no == inode_no){
            inode_found->i_open_cnt++;
            return inode_found;
        }
        elem = elem->next;
    }

    // 如果不存在, 则从磁盘中读取inode, 而后插入到打开的inode列表中
    inode_position_t inode_pos;
    inode_locate(partition, inode_no, &inode_pos);


    // 从内核的堆中分配内存, 来存储inode
    task_struct_t *cur = running_thread();
    uint32_t *cur_pgdir_backup = cur->pgdir;
    // 伪装当前进程为内核线程, 而后分配得到的内存就位于内核空间
    cur->pgdir = NULL;
    inode_found = (inode_t *) sys_malloc(sizeof(inode_t));
    cur->pgdir = cur_pgdir_backup;

    char *inode_buf;
    uint32_t sector_to_read = 0;
    if (inode_pos.multi_sec)
        sector_to_read = 2;
    else
        sector_to_read = 1;
    inode_buf = (char *) sys_malloc(SECTOR_SIZE * sector_to_read);
    ide_read(partition->my_disk, inode_pos.sec_lba, inode_buf, sector_to_read);
    // 将读出的数据复制到内核内存中的inode_t中
    memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(inode_t));

    // 将inode_found插入到open_inode list中
    list_push(&partition->open_inodes, &inode_found->inode_tag);
    inode_found->i_open_cnt = 1;

    // 释放用户内存空间的inode_buf
    sys_free(inode_buf);

    return inode_found;
}



/**
 * @brief inode_close用于关闭inode指向的inode. 具体来说, 该函数会在open_inode链表
 *      中删除inode指向的inode, 或者引用计数减1
 * 
 * @param inode 要关闭的inode
 */
void inode_close(inode_t *inode){
    // 操作共享数据前, 先关中断
    intr_status_t old_status = intr_disable();
    if (--inode->i_open_cnt == 0){
        list_remove(&inode->inode_tag);
        task_struct_t *cur = running_thread();
        uint32_t *cur_pagedir_backup = cur->pgdir;
        cur->pgdir = NULL;
        sys_free(inode);
        cur->pgdir = cur_pagedir_backup;
    }
    intr_set_status(old_status);
}


/**
 * @brief inode_delete用于删除磁盘上编号的为inode_no的inode中的信息. 其实此函数是没有必要的
 *      因为只要内存和磁盘中inode bitmap中清零了, 下次创建文件的时候就会覆盖. 
 *      因此, 该函数是调试时测试用的
 * 
 * @param partition 需要删除的inode所在的分区
 * @param inode_no 需要删除的inode在inode_table中的编号
 * @param io_buf 修改磁盘需要的缓冲区, 由调用者提供
 */
void inode_delete(partition_t *partition, uint32_t inode_no, void *io_buf){
    ASSERT(inode_no < 4096);

    // 首先inode在磁盘中的位置
    inode_position_t inode_pos;
    inode_locate(partition, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba <= (partition->start_lba + partition->sec_cnt));

    char *inode_buf = (char *)io_buf;
    // 1. 要删除的inode在磁盘上占用了多个分区
    if (inode_pos.multi_sec){
        // 1.1 将多个分区中的内容一起读出来
        ide_read(partition->my_disk, inode_pos.sec_lba, inode_buf, 2);
        // 1.2 单独删除inode_no的位置
        memset((inode_buf + inode_pos.off_size), 0, sizeof(inode_t));
        // 1.3 将多个分区中的内容写入到分区中
        ide_write(partition->my_disk, inode_pos.sec_lba, io_buf, 2);
    // 2. 要删除的inode在磁盘中只占用一个分区
    } else {
        // 2.1 将一个分区中的内容一起读出来
        ide_read(partition->my_disk, inode_pos.sec_lba, inode_buf, 1);
        // 2.2 单独删除inode_no的位置
        memset((inode_buf + inode_pos.off_size), 0, sizeof(inode_t));
        // 2.3 将一个分区中的内容写入到磁盘中
        ide_write(partition->my_disk, inode_pos.sec_lba, io_buf, 1);
    }
}

extern partition_t *current_partition;

/**
 * @brief inode_release用于删除partition分区中编号为inode的文件占用的所有块
 * 
 * @param partition 需要删除的inode所在的分区
 * @param inode_no 需要删除的inode的编号
 */
void inode_release(partition_t *partition, uint32_t inode_no){
    // 避免误删
    inode_t *inode_to_delete = inode_open(partition, inode_no);
    ASSERT(inode_to_delete->i_no == inode_no);

    // 1. 回收inode占用的所有的块
    uint8_t block_idx = 0, block_cnt = 12;
    uint32_t block_bitmap_idx;
    uint32_t all_blocks_lba[140] = {0};

    // 现将文件前12个块地址读入
    while (block_idx < 12){
        all_blocks_lba[block_idx] = inode_to_delete->i_sectors[block_idx];
        block_idx++;
    }

    // 如果文件使用了一级间接块, 那么还需要读入一级间接块
    if (inode_to_delete->i_sectors[12] != 0){
        // 读入一级块中的lba地址到all_blocks_lba中
        ide_read(partition->my_disk, inode_to_delete->i_sectors[12], all_blocks_lba + 12, 1);
        block_cnt = 140;

        // 回收一级间接块, 即置空
        block_bitmap_idx = inode_to_delete->i_sectors[12] - partition->sb->data_start_lba;
        ASSERT(block_bitmap_idx > 0);
        // 先把内存中的block bitmap置空
        bitmap_set(&partition->block_bitmap, block_bitmap_idx, 0);
        // 再同步到磁盘中
        bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);
    }

    // 回收文件占用的所有block
    block_idx = 0;
    while (block_idx < block_cnt){
        if (all_blocks_lba[block_idx] != 0){
            block_bitmap_idx = 0;
            block_bitmap_idx = all_blocks_lba[block_idx] - partition->sb->data_start_lba;
            ASSERT(block_bitmap_idx > 0);
            bitmap_set(&partition->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);
        }
        block_idx++;
    }

    // 回收文件对应的inode占用inode号
    bitmap_set(&partition->inode_bitmap, inode_no, 0);
    bitmap_sync(current_partition, inode_no, INODE_BITMAP);
    
    // 其实不需要磁盘清零的, 因为会覆盖
    void *io_buf = sys_malloc(1024);
    inode_delete(partition, inode_no, INODE_BITMAP);
    sys_free(io_buf);

    inode_close(inode_to_delete);
}


/**
 * @brief inode_init用于初始化new_inode指向的块
 * 
 * @param inode_no new_inode指向的块在inode_table中的index
 * @param new_inode 指向需要初始化的块
 */
void inode_init(uint32_t inode_no, inode_t *new_inode){
    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    new_inode->i_open_cnt = 0;
    new_inode->write_deny = false;

    // 初始化inode中的索引
    uint8_t sec_idx = 0;
    while (sec_idx < 13)
        new_inode->i_sectors[sec_idx++] = 0;
}