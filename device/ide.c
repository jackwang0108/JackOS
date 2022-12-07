#include "io.h"
#include "ide.h"
#include "kstdio.h"
#include "stdio.h"
#include "debug.h"
#include "timer.h"
#include "string.h"
#include "memory.h"
#include "interrupt.h"

// 硬盘各个寄存器的端口
#define reg_data(channel)           (channel->port_base + 0)
#define reg_error(channel)          (channel->port_base + 1)
#define reg_sect_cnt(channel)       (channel->port_base + 2)
#define reg_lba_low(channel)        (channel->port_base + 3)
#define reg_lba_middle(channel)     (channel->port_base + 4)
#define reg_lba_high(channel)       (channel->port_base + 5)
#define reg_dev(channel)            (channel->port_base + 6)
#define reg_status(channel)         (channel->port_base + 7)
#define reg_cmd(channel)            reg_status(channel)
#define reg_alt_status(channel)     (channel->port_base + 0x206)
#define reg_ctl(channel)            reg_alt_status(channel)


// reg_status寄存器的一些关键位
#define BIT_STAT_BSY                0b10000000      ///< disk is busy
#define BIT_STAT_DRDY               0b00000100      ///< data is read (disk finshed reading data)
#define BIT_STAT_DRQ                0b00001000      ///< disk is ready to transfer data

// device寄存器的一些关键位
#define BIT_DEV_MBS                 0b10100000      ///< 5-th and 7-th bit is fixed to 1
#define BIT_DEV_LBA                 0b01000000
#define BIT_DEV_DEV                 0b00010000

// cmd寄存器的一些指令
#define CMD_IDENTITY                0b11101100      ///< identity 指令
#define CMD_READ_SECTOR             0b00100000      ///< read sector指令
#define CMD_WRITE_SECTOR            0b00110000      ///< write sector指令

// 可读写的最大区间
#define max_lba                     ((80 * 1024 * 1024 / 512) - 1)      // maximum read sectors (unit in parentheses) ((80M * 1024 * 1024)(bits) / 512 (bits/sector)) - 1 

void select_disk(disk_t *hd);
void select_sector(disk_t *hd, uint32_t lba, uint8_t sec_cnt);
void cmd_out(ide_channel_t *channel, uint8_t cmd);
void read_from_sector(disk_t *hd, void *buf, uint8_t sec_cnt);
void write_to_sector(disk_t *hd, void *buf, uint8_t sec_cnt);
bool busy_wait(disk_t *hd);
static void identify_disk(disk_t *hd);
void swap_paired_bytes(const char* src, char* dst, uint32_t len);
static void partition_scan(disk_t *hd, uint32_t ext_lba);
static bool partition_info(list_elem_t *elem, int unused_arg);

uint8_t channel_cnt;
ide_channel_t channels[8];          ///< 系统当前最大支持两个ide通道

/**
 * @brief 硬盘中断处理函数
 * 
 * @param vec_no 中断引脚号
 */
void intr_hd_handler(uint8_t vec_no){
    ASSERT(vec_no == 0x2E || vec_no == 0x2F);
    uint8_t channel_no = vec_no - 0x2e;
    ide_channel_t *channel = &channels[channel_no];
    ASSERT(channel->vec_no == vec_no);

    // 当硬盘读取完毕, 硬盘控制器会发送中断来, 所以一定之前已经进行了cmd_out, 此时expecting_intr一定为true
    if (channel->expecting_intr){
        channel->expecting_intr = false;
        sema_up(&channel->disk_done);
        inb(reg_status(channel));
    }
}


uint8_t p_no = 0, l_no = 0;         ///< 硬盘主分区和逻辑分区记录
list_t partition_list;              ///< 磁盘中分区表的链表

/**
 * @brief ide_init用于初始化硬盘
 * 
 */
void ide_init(void){
    kprintf("ide_init start\n");
    uint8_t hd_cnt = *((uint8_t*) 0x475);           // BIOS将检测到的硬盘数量写入到0x475处
    ASSERT(hd_cnt > 0 );
    channel_cnt = DIV_CEILING(hd_cnt, 2);           // 根据硬盘数量计算系统当前可用的通道数


    // 初始化每个通道上的硬盘
    uint8_t channel_no = 0;
    ide_channel_t *channel;
    while (channel_no < channel_cnt){
        channel = &channels[channel_no];
        sprintf(channel->name, "ide%d", channel_no);

        switch (channel_no){
            case 0:
                channel->port_base = 0x1F0;
                channel->vec_no = 0x20 + 14;
                break;
            case 1:
                channel->port_base = 0x170;
                channel->vec_no = 0x20 + 15;
                break;
            default:
                PANIC("only 2 ide is supported now!");
                break;
        }

        channel->expecting_intr = false;
        mutex_init(&channel->mutex);
        sema_init(&channel->disk_done, 0);

        register_handler(channel->vec_no, intr_hd_handler);


        // 一个通道上有两个硬盘, 输出两个硬盘上的分区表
        uint8_t dev_no = 0;
        while (dev_no < 2){
            disk_t *hd = &channel->devices[dev_no];
            hd->my_channel = channel;
            hd->dev_no = dev_no;
            sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
            // 读取硬盘上的分区信息
            identify_disk(hd);
            if (dev_no != 0)
                partition_scan(hd, 0);
            // 重置p_no和l_no
            p_no = l_no = 0;
            dev_no++;
        }

        channel_no++;
    }

    kprintf("    Partition info:\n");
    list_traversal(&partition_list, partition_info, (int)NULL);
    kprintf("ide_init done\n");
}


/**
 * @brief select_disk用于设置需要读写的硬盘
 * 
 * @param hd 指向需要读写的硬盘的指针
 */
void select_disk(disk_t *hd){
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    if (hd->dev_no == 1)
        reg_device |= BIT_DEV_DEV;       // 从盘dev位需要设置为1
    outb(reg_dev(hd->my_channel), reg_device);
}


/**
 * @brief select_sector用于设置需要读写的扇区数
 * 
 * @param hd 指向需要读写的硬盘的指针
 * @param lba 开始读写的扇区号
 * @param sec_cnt 需要读写的扇区数
 */
void select_sector(disk_t *hd, uint32_t lba, uint8_t sec_cnt){
    ASSERT(lba <= max_lba);
    ide_channel_t *channel = hd->my_channel;

    // 写入需要读写的扇区数
    outb(reg_sect_cnt(channel), sec_cnt);

    // 写入开始读写的扇区号
    outb(reg_lba_low(channel), lba);
    outb(reg_lba_middle(channel), lba >> 8);
    outb(reg_lba_high(channel), lba >> 16);

    // 写入需要读写的扇区号24~27位
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}



/**
 * @brief cmd_out用于向channel通道发送指令cmd
 * 
 * @param channel 需要发送指令的通道
 * @param cmd 需要发送的命令
 */
void cmd_out(ide_channel_t *channel, uint8_t cmd){
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);
}


/**
 * @brief read_from_sector用于从hd指向的硬盘中读取sec_cnt个扇区到buf缓冲区中
 * 
 * @param hd 指向需要读取数据的硬盘
 * @param buf 被读取的数据将保存的缓冲区
 * @param sec_cnt 需要读取的扇区数
 */
void read_from_sector(disk_t *hd, void *buf, uint8_t sec_cnt){
    uint32_t size_in_byte;
    if (sec_cnt == 0)
        size_in_byte = 256 * 512;           // overflow, 256 equals to 0
    else
        size_in_byte = sec_cnt * 512;
    insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}


/**
 * @brief write_to_sector用于将buf中的数据写入到hd指向的硬盘中连续sec_cnt个扇区
 * 
 * @param hd 指向需要写入数据的硬盘
 * @param buf 被写入的数据所在的缓冲区
 * @param sec_cnt 需要写入的扇区数
 */
void write_to_sector(disk_t *hd, void *buf, uint8_t sec_cnt){
    uint32_t size_in_byte = 0;
    if (sec_cnt == 0)
        size_in_byte = 256 * 512;
    else
        size_in_byte = sec_cnt * 512;
    outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}


/**
 * @brief busy_wait将等待30秒以等待硬盘读取完毕, 等待期间将以10毫秒为间隔轮询硬盘
 * @param hd 需要等到的硬盘
 */
bool busy_wait(disk_t *hd){
    ide_channel_t *channel = hd->my_channel;
    uint16_t time_limit = 30 * 1000;
    while (time_limit -= 10 > 0){
        if (!(inb(reg_status(channel)) & BIT_STAT_BSY))
            return inb(reg_status(channel)) & BIT_STAT_DRQ;
        else
            mtime_sleep(10);
    }
    return false;
}


/**
 * @brief ide_read用于在hd指向的硬盘中从lba扇区号开始读取连续的sec_cnt个扇区, 并将数据存储到buf中
 * 
 * @param hd 只想将要读取的硬盘
 * @param lba 开始读取的扇区号
 * @param buf 读取的数据将要写入的缓冲区
 * @param sec_cnt 需要读取的扇区号
 */
void ide_read(disk_t *hd, uint32_t lba, void *buf, uint32_t sec_cnt){
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    
    // 操作共享数据, 需要上锁
    mutex_acquire(&hd->my_channel->mutex);

    // 1. 选择需要操作的硬盘
    select_disk(hd);

    uint32_t secs_done = 0;
    uint32_t secs_to_read = 0;
    while (secs_done < sec_cnt){
        secs_to_read = (secs_done + 256) <= sec_cnt ? 256 : sec_cnt - secs_done;

        // 2. 写入待读入的扇区数和起始扇区号
        select_sector(hd, lba + secs_done, secs_to_read);

        // 3. 写入读取命令
        cmd_out(hd->my_channel, CMD_READ_SECTOR);

        // 此后, 硬盘开始读取数据, 线程阻塞自己, 等待硬盘读取完毕
        sema_down(&hd->my_channel->disk_done);

        // 4. 轮询等待硬盘读取完毕
        if (!busy_wait(hd)){
            char error[64];
            sprintf(error, "%s read sector %d failed!\n", hd->name, lba);
            PANIC(error);
        }

        // 5. 从磁盘的缓冲区中读取数据
        read_from_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_to_read);

        secs_done += secs_to_read;
    }

    mutex_release(&hd->my_channel->mutex);
}



/**
 * @brief ide_write用于将buf中的数据写入到hd指向的硬盘中从lab扇区号开始的连续sec_cnt个扇区
 * 
 * @param hd 指向需要写入的硬盘
 * @param lba 开始读取的lba扇区号
 * @param buf 将要写入到硬盘中的数据
 * @param sec_cnt 需要写入的扇区数
 */
void ide_write(disk_t *hd, uint32_t lba, void *buf, uint32_t sec_cnt){
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);
    
    // 操作共享数据, 需要上锁
    mutex_acquire(&hd->my_channel->mutex);

    // 1. 选择需要操作的硬盘
    select_disk(hd);

    uint32_t secs_done = 0;
    uint32_t secs_to_write = 0;
    while (secs_done < sec_cnt){
        secs_to_write = (secs_done + 256) <= sec_cnt ? 256 : sec_cnt - secs_done;

        // 2. 写入待写入的扇区数和起始扇区号
        select_sector(hd, lba + secs_done, secs_to_write);

        // 3. 写入读取命令
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);


        // 4. 轮询等待硬盘写入完毕
        if (!busy_wait(hd)){
            char error[64];
            sprintf(error, "%s write sector %d failed!\n", hd->name, lba);
            PANIC(error);
        }

        // 5. 将磁盘的缓冲区中数据写入到硬盘中
        write_to_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_to_write);


        // 此后, 硬盘开始读取数据, 线程阻塞自己, 等待硬盘写入完毕
        sema_down(&hd->my_channel->disk_done);

        secs_done += secs_to_write;
    }
    mutex_release(&hd->my_channel->mutex);
}



/// @brief 分区表结构体
typedef struct __partition_table_entry_t {
    uint8_t booable;                // 当前分区是否包含系统loader(当前分区是否可引导)
    uint8_t start_head;             // 当前分区起始磁头号
    uint8_t start_sec;              // 当前分区起始扇区号
    uint8_t start_chs;              // 当前分区起始柱面号
    uint8_t fs_type;                // 当前分区文件系统
    uint8_t end_head;               // 当前分区结束磁头号
    uint8_t end_sec;                // 当前分区结束扇区号
    uint8_t end_chs;                // 当前分区结束柱面号
    uint32_t start_lba;             // 当前分区起始扇区LBA地址
    uint32_t sec_cnt;               // 当前分区的扇区数
}__attribute__((packed)) partition_table_entry_t;            // 禁止gcc对齐


/// @brief 引导扇区(MBR或EBR)结构体
typedef struct __boot_sector_t {
    uint8_t boot_code[446];                         // 引导扇区代码
    partition_table_entry_t partition_table[4];     // 分区表
    uint16_t magic_num;                             // 引导扇区结束魔数0x55AA, x86小端序, 在内存中是 0xAA, 0x55
}__attribute__((packed)) boot_sector_t;                      // 禁止gcc对齐


/**
 * @brief swap_paired_bytes将长度为len的src视为字节对数组后, 将字节对互换得到的新字节对存入dst中. 不会改变src中的数据

 * @example Before: src = [0x01, 0x02, 0x03, 0x04], dst = [0x00, 0x00, 0x00, 0x00]
 *          Call: swap_paired_bytes(src, dst, 4)
 *          After: src = [0x01, 0x02, 0x03, 0x04], dst = [0x02, 0x01, 0x04, 0x03]
 * 
 * @param src 
 * @param dst 
 * @param len 
 */
void swap_paired_bytes(const char* src, char* dst, uint32_t len){
    uint8_t idx;
    for (idx = 0; idx < len; idx += 2){
        // 因为dst是值复制, 所以主调函数中的src的值不会变
        dst[idx + 1] = *src++;
        dst[idx] = *src++;
    }
    dst[idx] = '\0';
}


/**
 * @brief identify_disk用于读取某个磁盘的基础信息, 并且输出到屏幕上
 * 
 * @param hd 指向需要读取分区信息的磁盘
 */
void identify_disk(disk_t *hd){
    char id_info[512];
    select_disk(hd);
    cmd_out(hd->my_channel, CMD_IDENTITY);

    // 等待硬盘控制器准备硬盘参数信息
    sema_down(&hd->my_channel->disk_done);

    if(!busy_wait(hd)){
        char error[64];
        sprintf(error, "%s identify failed!\n", hd->name);
        PANIC(error);
    }

    // 将硬盘hd准备好的数据(参数信息)写入到id_info数组中
    read_from_sector(hd, id_info, 1);

    char buf[64];
    // defined in ata manual
    uint8_t mode_len = 40,
            mode_start = 27 * 2,
            serial_number_len = 20,
            serial_number_start = 10 * 2;
    // 输出信息, x86是小端序, 因此需要进行转换
    swap_paired_bytes(&id_info[serial_number_start], buf, serial_number_len);
    kprintf("    disk %s:\n", hd->name);
    kprintf("         SN: %s\n", buf);
    memset(buf, 0, sizeof(buf));
    swap_paired_bytes(&id_info[mode_start], buf, mode_len);
    kprintf("         Module: %s\n", buf);

    uint32_t sectors = *(uint32_t*) &id_info[60 * 2];
    kprintf("         Secotrs: %d\n", sectors);
    kprintf("         Capacity: %d MB\n", sectors * 512 / 1024 / 1024);
}


int32_t ext_lba_base = 0;           ///< 用于记录总扩展分区的起始lba, 用于partition_scan

/**
 * @brief partition_scan是一个递归函数, 该函数会递归的扫描hd指向的硬盘中的所有分区表, 并将分区表扇区作为节点插入到partition_list中
 * 
 * @param hd 要扫描的硬盘
 * @param ext_lba 分区表所在的扇区号, 第一次被调用时要设置为0, 即硬盘的MBR所在的扇区号
 */
void partition_scan(disk_t *hd, uint32_t ext_lba){

    // read boot sector
    boot_sector_t *bs = sys_malloc((uint32_t)sizeof(boot_sector_t));
    ide_read(hd, ext_lba, (void*)bs, 1);

    // 遍历4个分区表
    uint8_t partition_idx = 0;
    partition_table_entry_t * partition = bs->partition_table;
    while (partition_idx++ < 4){
        // 处理当前这个分区表记录
        if (partition->fs_type == 0x5){             // 扩展分区
            if (ext_lba_base != 0)
                // 
                partition_scan(hd, partition->start_lba + ext_lba_base);
            else {
                // 第一次读取扩展分区
                ext_lba_base = partition->start_lba;
                partition_scan(hd, partition->start_lba);
            }
        } else if (partition->fs_type != 0) {       // 非扩展分区(主分区 / 逻辑分区)
            if (ext_lba == 0) {                     // 主分区
                // 当前分区是主分区, 则将当前主分区信息写入到硬盘上的分区表中
                hd->prim_parts[p_no].start_lba = ext_lba + partition->start_lba;
                hd->prim_parts[p_no].sec_cnt = partition->sec_cnt;
                hd->prim_parts[p_no].my_disk = hd;
                // 插入系统的分区队列中
                list_append(&partition_list, &hd->prim_parts[p_no].part_tag);
                // 记录分区信息
                sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no + 1);
                p_no++;
                ASSERT(p_no < 4);
            } else {                                // 逻辑分区
                hd->logic_parts[l_no].start_lba = ext_lba + partition->start_lba;
                hd->logic_parts[l_no].sec_cnt = partition->sec_cnt;
                hd->logic_parts[l_no].my_disk = hd;
                // 插入系统的分区队列中
                list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
                // 记录分区信息
                sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name, l_no + 5);        // 主分区是1-4, 逻辑分区从5开始
                l_no++;
                if (l_no >= 8){
                    kprintf("Overmuch logic partition detected! Only support 8 logic partiton now!");
                    return;
                }
            }
        }

        // 处理到下一个分区表中的记录
        partition++;
    }
    sys_free((void*)bs);
}


/**
 * @brief partition_info用于输出系统的分区链表中每一个分区的信息
 * 
 * @param elem 分区的elem
 * @return true 为list_traversal准备, 无实际含义
 * @return false 为list_traversal准备, 无实际含义
 */
bool partition_info(list_elem_t *elem, UNUSED int unused_arg){
    partition_t *part = elem2entry(partition_t, part_tag, elem);
    kprintf("        %s start lba: 0x%x, sec_cnt: 0x%x\n", part->name, part->start_lba, part->sec_cnt);
    return false;
}
