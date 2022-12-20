#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "stdint.h"
#include "list.h"
#include "bitmap.h"
#include "sync.h"
#include "super_block.h"


/// @brief Sturcture of Disk Partition, only in memory. Initialized in init_all() and
///        filled in mount_partition()
typedef struct __partition_t{
    uint32_t start_lba;                     ///< 当前分区的起始扇区LBA号
    uint32_t sec_cnt;                       ///< 当前分区占用的扇区数
    struct __disk_t *my_disk;               ///< 当前分区所属的硬盘
    list_elem_t part_tag;                   ///< 当前分区在链表中的标记
    char name[8];                           ///< 当前分区的名字, 例如sda1, sda2
    struct __super_block_t *sb;             ///< 当前分区的超级块, 后面文件系统会用到
    bitmap_t block_bitmap;                  ///< 当前分区在内存中的块位图
    bitmap_t inode_bitmap;                  ///< 当前分区在内存中的block位图
    list_t open_inodes;                     ///< 当前分区打开的inode的标记, 后面文件系统会用到
} partition_t;


/// @brief 硬盘结构
typedef struct __disk_t {
    char name[8];                           ///< 当前硬盘名称
    struct __ide_channel_t *my_channel;     ///< 当前硬盘归属于的ide通道
    uint8_t dev_no;                         ///< 当前硬盘是主硬盘还是从硬盘, 主为0, 从为1
    partition_t prim_parts[4];              ///< 当前硬盘的主分区表, 即MBR中的主分区表, 最多四个主分区
    partition_t logic_parts[8];             ///< 当前硬盘的逻辑分区数, 理论上无限, 目前设置上限为8
} disk_t;


/// @brief ata通道结构
typedef struct __ide_channel_t {
    char name[8];                           ///< 当前ata通道名称
    /**
     * @brief 当前通道的起始端口号:
     *      1. 通道1命令寄存器是0x1F0~0x1F7, 控制寄存器是0x3F6, 其端口号是0x1F0, 偏移是0~7, 0x206
     *      2. 通道2命令寄存器是0x170~0x177, 控制寄存器是0x376, 其端口号是0x170, 偏移是0~7, 0x206
     */
    uint16_t port_base;
    uint8_t vec_no;                         ///< 当前通道所用的中断号
    /**
     * @brief 当前通道的锁, 一个通道有两个硬盘, 两个硬盘共用一个中断
     *        因此需要用锁来表明当前中断相应的是哪个硬盘. 即访问硬盘1
     *        时候给通道上锁, 这样就避免了硬盘1读取数据区间用户操作硬盘2
     *        导致当硬盘1读取完成后发送中断时, CPU不知道该读取那个硬盘
     */
    mutex_t mutex;
    /// @brief 表示当前通道正在等待硬盘的锁, 用于帮助中断处理程序判断本次中断是否表示硬盘读取数据完毕, 可以开始传输数据
    bool expecting_intr;
    /// @brief 硬盘工作期间访问的进程将被阻塞在此
    semaphore_t disk_done;
    disk_t devices[2];                      ///< 一个通道上连接的两个硬盘, 一个是主盘, 一个是从盘
} ide_channel_t;


extern ide_channel_t channels[8];          ///< 系统当前最大支持两个ide通道

/**
 * @brief ide_init用于初始化硬盘
 * 
 */
void ide_init(void);

/**
 * @brief 硬盘中断处理函数
 * 
 * @param vec_no 中断引脚号
 */
void intr_hd_handler(uint8_t vec_no);


/**
 * @brief ide_read用于在hd指向的硬盘中从lba扇区号开始读取连续的sec_cnt个扇区, 并将数据存储到buf中
 * 
 * @param hd 只想将要读取的硬盘
 * @param lba 开始读取的扇区号
 * @param buf 读取的数据将要写入的缓冲区
 * @param sec_cnt 需要读取的扇区号
 */
void ide_read(disk_t *hd, uint32_t lba, void *buf, uint32_t sec_cnt);



/**
 * @brief ide_write用于将buf中的数据写入到hd指向的硬盘中从lab扇区号开始的连续sec_cnt个扇区
 * 
 * @param hd 指向需要写入的硬盘
 * @param lba 开始读取的lba扇区号
 * @param buf 将要写入到硬盘中的数据
 * @param sec_cnt 需要写入的扇区数
 */
void ide_write(disk_t *hd, uint32_t lba, void *buf, uint32_t sec_cnt);



#endif