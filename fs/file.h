#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "stdint.h"
#include "inode.h"
#include "dir.h"

/// 系统最大可以同时打开32个文件
#define MAX_FILE_OPEN 32

typedef struct __file_desc_t {
    uint32_t fd_pos;
    uint32_t fd_flag;
    inode_t *fd_inode;
} file_desc_t;


typedef enum __std_fd {
    stdin_no,
    stdout_no,
    stderr_no
} std_fd;


typedef enum __bitmap_type_t {
    INODE_BITMAP,
    BLOCK_BITMAP
} bitmap_type_t;


extern file_desc_t file_table[MAX_FILE_OPEN];


/**
 * @brief get_free_slot_in_global用于从全局文件描述符表中找到一个空位
 * 
 * @return int32_t 若全局文件描述符表中有空闲的文件描述符, 则返回空闲的文件描述符表的索引; 
 *          若无空闲的文件描述符, 则返回-1
 */
int32_t get_free_slot_in_global(void);



/**
 * @brief pcb_fd_install用于将全局文件描述符索引安装到用户进程的文件描述符表中
 * 
 * @param globa_fd_idx 要安装的全局文件描述符表
 * @return int32_t 若成功安装到用户文件描述符表中, 则返回用户文件描述符表的索引, 若失败则返回-1
 */
int32_t pcb_fd_install(int32_t globa_fd_idx);



/**
 * @brief inode_bitmap_alloc用于从partition的inode列表中分配一个inode
 * 
 * @param partition 需要分配inode的分区
 * @return int32_t 若分配成功, 则返回分配的得到的inode的index; 若分配失败, 则返回-1
 */
int32_t inode_bitmap_alloc(partition_t *partition);



/**
 * @brief block_bitmap_alloc用于从partition的空间block中分配一个block
 * 
 * @param partition 需要分配block的分区
 * @return int32_t 若分配成功, 则返回分配的得到的block的lba扇区号; 若分配失败, 则返回-1
 */
int32_t block_bitmap_alloc(partition_t *partition);


/**
 * @brief bitmap_sync用于将内存中的bitmap的的bit_idx为写入到partition指向的分区的中的inode bitmap或者block bitmap
 * 
 * @param partition 要写入的bitmap所在的分区
 * @param bit_idx 要写入的位
 * @param btmp 要写入的位图
 */
void bitmap_sync(partition_t *partition, uint32_t bit_idx, uint8_t btmp);



/**
 * @brief file_create用于在parent_dir执行的目录中创建一个名为filename的一般文件. 注意, 创建好的文件默认处于打开状态 
 *        因此创建的文件对应的inode会被插入到current_partition.open_inode_list, 而且还会把刚打开的文件添加到系统的文件描述符表中
 * 
 * @details 创建一个普通文件, 需要如下的几步:
 *              1. 在当前分区中申请一个inode, 此时首先需要向inode_bitmap中申请获得一个inode号, 然后才能操作inode_table
 *              2. 申请得到inode之后, 需要填充inode, 别的信息不说了, 重要的是要填充文件的数据块, 即填充inode.i_sector
 *                 因为其中也是块, 所以首先需要: 向block_bitmap中申请一个block, 然后才能操作对应的block
 *              3. 创建文件之后, 需要向文件所在的父目录中, 插入文件的目录项
 *              4. 因为前面操作的inode_bitmap, block_bitmap等等都是在内存中的, 因此需要将内存中的block_bitmap和inode_bitmap
 *                 同步到磁盘中
 *              5. 如果中间哪步出错, 那么申请的资源一定释放, 即inode_bitmap或者block_bitmap中修改的位需要修改回来
 * 
 * @param parent_dir 需要创建文件所在的目录
 * @param filename 需要创建的文件的名字
 * @param flag 
 * @return int32_t 若成功, 则返回文件描述符; 若失败, 则返回-1
 */
int32_t file_create(dir_t *parent_dir, char *filename, uint8_t flag);



/**
 * @brief file_open用于打开inode编号为inode_no的inode所表示的文件. 具体来说, 该函数干的事:
 *          1. 在系统全局的打开文件列表(全局文件描述符表)中找到一个空位, 并对其进行初始化
 *          2. 价格全局文件描述符表中的空位的index插入到进程的打开文件列表(进程文件描述符表)中
 *        最终该函数返回进程文件描述符表中的文件描述符
 * 
 * @param inode_no 需要打开的文件的inode
 * @param flag 打开文件的标识, 例如如果指定为O_CREAT, 则文件如果不存在, 就会创建
 * @return int32_t 进程的文件描述符表
 */
int32_t file_open(uint32_t inode_no, uint8_t flag);



/**
 * @brief file_close用于关闭file指向的文件
 * 
 * @param file 需要关闭的文件的文件描述符
 * @return int32_t 
 */
int32_t file_close(file_desc_t *file);

/**
 * @brief file_write用于将buf中的count个字节写入到file指向的文件
 * 
 * @param file 需要写入的文件
 * @param buf 需要写入文件的数据
 * @param count 需要写入的字节数
 * @return int32_t 若写入成功则返回写入的字节数; 若失败则返回-1
 */
int32_t file_write(file_desc_t *file, const void *buf, uint32_t count);



/**
 * @brief file_read用于从file指向的文件从读取count个字节到buf中
 * 
 * @param file 需要读取的文件
 * @param buf 读取的文件将写入的内存缓存
 * @param count 需要读取的字节数
 * @return int32_t 若读取成功, 则返回读取的字节数; 若失败, 则返回-1
 */
int32_t file_read(file_desc_t *file, void *buf, uint32_t count);

#endif