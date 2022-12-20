#include "fs.h"
#include "ide.h"
#include "dir.h"
#include "file.h"
#include "inode.h"
#include "debug.h"
#include "string.h"
#include "kstdio.h"
#include "ioqueue.h"
#include "console.h"


//   Layout of Physical Disk:
//          1. There is one Master Boot Sector, MBR
//          2. In MBR, 
//                 ,-------------------------------------------------------------------------------------------------------------------------------------------,
//                 |  MBR Boot Sector | Primary Partition 1 | Primary Partition 2 | Primary Patition 3 |             Extended   Partition                      |
//                 '-------------------------------------------------------------------------------------------------------------------------------------------'
//                                    |                     |                                         /                                                         \ . 
//                                    |                     |             ,--------------------------'                                                           '------------------------------------------------,
//                                    |                     |            /                                                                                                                                         \ . 
//                                    |                     |           ,-------------------------------------------------------------------------------------------------------------------------------------------,
//                                   /                       \          |          Logical Partition 1          |          Logical Partition 2          |          Logical Partition 3          |         ......    |
//                                  /                         \         '-------------------------------------------------------------------------------------------------------------------------------------------'
//                                 /                           \ . 
//           ,--------------------'                             `--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------,
//          /       To illustrate, each Block in illustration consists of 3 Sector, But each block actually consists of 1 sector in JackOS                                                                                                              \ . 
//          ,--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------      ------,
//          |        OS Loader Block         |          Super Block          |  Free Block Bitmap  |    Inode Bitmap      |                   Inode Table                 |          Root Dir Block           |             Free Block Area ......       |
//          |--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------      ------|
//          | Sector 1 | Sector 2 | Sector 3 | Sector 4 | Sector 5| Sector 6 | Sector 7 | Sector 8 | Sector 9 | Sector 10 | Sector 11 | Sector 12 | Sector 13 | Sector 14 | Sector 15 | Sector 16 | Sector 17 | Sector 18 | Sector 19 | Sec ......       |
//          '--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------      ------'
// 
// 
//   A smaller view of File system Layout:
//  ,------------------------------------------------------------------------------------------------------------------ ...... -------,
//  | OS Loader Block | Super Block | Free Block Bitmap | Inode Bitmap | Inode Table | Root Dir Block |       Free Block Area         |   Next Partition
//  ,------------------------------------------------------------------------------------------------------------------ ...... -------'

/**
 * @brief partition_format用于对hd指向的硬盘中的patition分区进行格式化. 
 *      注意, 本系统中的文件系统, 一个块占用一个扇区, 因此block_cnt和sec_cnt是等价的
 * 
 * @details 一个分区的文件系统包含: 
 *              1. OS Loader Block          可能占用多个块, 但是我们的系统中loader占用1个块
 *              2. Super Block              记录文件系统元信息的超级块, 同样, 别的系统可能会占用多个块, 但是我们的系统的超级块只会占用一个扇区
 *              3. Block Bitmap             Block bitmap的位数取决于磁盘剩余多少个块, 所以同样长度不确定, 可能占用1~多个扇区
 *              4. Inode Bitmap             Inode bitmap的位数取决于文件系统的设定, 我们设定JackOS中只有4096个块, 因此只会使用一个扇区
 *              5. Inode Table              一个Inode占用一个扇区, 所以这里会占用4096个扇区
 * 
 *          因此, 为了在一个分区上创建文件系统, 本函数干的事情如下:
 *              1. 略过OS Loader, 这个是不该由格式化函数来写入到磁盘中
 *              2. 计算本文件系统的元信息, 并将其写入到Super Block中
 *              3. Block Bitmap初始化, 一开始只分配出去了根目录的inode所在块, 所以写入的block bitmap就是10000000.......
 *              4. Inode Bitmap初始化, 一开始分配出去的inode只有根目录的inode, 所以写入的inode bitmap就是1000000.......
 *              5. Inode Table初始化, 同样, 第一个inode就是根目录, 其他的全都设置为0
 * 
 * @param hd 指向需要格式化的分区所在的硬盘
 * @param partition 指向需要格式化的分区
 */
static void partition_format(partition_t *partition){
    // 系统loader所在的块(启动扇区)占用的扇区数
    uint32_t boot_sector_sects = 1;
    // 超级块占用的扇区数
    uint32_t super_block_sects = 1;
    // inode_bitmap占用的扇区数
    uint32_t inode_bitmap_sects = DIV_CEILING(MAX_FILE_PER_PARTITION, BITS_PER_SECTOR);
    // inode table占用扇区数
    uint32_t inode_table_sects = DIV_CEILING(sizeof(inode_t) * MAX_FILE_PER_PARTITION, SECTOR_SIZE);

    // 计算已经占用和空闲的扇区数
    uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
    uint32_t free_sects = partition->sec_cnt - used_sects;


    // 计算block的bitmap的bit数和存储block bitmap占用的扇区数
    uint32_t block_bitmap_sects = DIV_CEILING(free_sects, BITS_PER_SECTOR);
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
    block_bitmap_sects = DIV_CEILING(block_bitmap_bit_len, BITS_PER_SECTOR);

    // 1. 在内存中创建超级块
    // 初始化超级块基础信息
    super_block_t sb;
    sb.magic = 0x20010107;                                      // My birthday :)
    sb.sec_cnt = partition->sec_cnt;
    sb.inode_cnt = MAX_FILE_PER_PARTITION;
    sb.partition_lba_base = partition->start_lba;
    // 初始化超级块中的block bitmap信息
    sb.block_bitmap_lba = sb.partition_lba_base + 2;            // 第0个块是MBR或者EBR, 第1个块是超级块
    sb.block_bitmap_sects = block_bitmap_sects;
    // 初始化超级块中的inode bitmap信息
    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sects;
    // 初始化超级块中的inode table信息
    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects;
    // 自由数据区
    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;

    // 剩余数据区
    sb.root_inode_no = 0;                                       // inode数组中的第一个inode将用于root dir
    sb.dir_entry_size = sizeof(dir_entry_t);

    // 2. 输出超级块元信息
    kprintf("------------------------------------------------------------------------------\n");
    kprintf("Partition: %s, magic: 0x%x\n", partition->name, sb.magic);
    kprintf("    all_sectors: 0x%x, inode_cnt: 0x%x\n", sb.sec_cnt, sb.inode_cnt);
    kprintf(
        "    block_btmp_sects: 0x%x, inode_btmp_sects: 0x%x, inode_table_sects: 0x%x\n", 
        sb.block_bitmap_sects, sb.inode_bitmap_sects, sb.inode_table_sects
    );
    kprintf(
        "    block_btmp_lba: 0x%x, inode_btmp_lba: 0x%x\n", 
        sb.block_bitmap_lba, sb.inode_bitmap_lba
    );
    kprintf(
        "    inode_table_lba: 0x%x, data_start_lba: 0x%x\n",
        sb.inode_table_lba, sb.data_start_lba
    );


    // 3. 将内存中的超级块信息写入到磁盘中
    disk_t *hd = partition->my_disk;
    ide_write(hd, partition->start_lba + 1, &sb, 1);                // 将超级块写入到第1个块, 但是因为JackOS中一个块只有一个扇区, 因此写入一个扇区即可
    kprintf("    super_block_lba: 0x%x\n", partition->start_lba + 1);

    // 4. 写入块位图和inode位图
    // 4.1 首先找到块位图和inode位图可能占用的最大内存, 然后分配内存
    uint32_t buf_size = sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects;
    buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;
    uint8_t *buf = sys_malloc(buf_size);

    // 4.2 写入块位图
    buf[0] |= 0x01;           // 目前所有空闲的块中, 第0个块被分配给了根目录
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
    uint32_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
    // 位图不一定占用完整个块, 所以这里先计算一下块位图占用的最后一个块中剩余多少个字节
    // 然后把剩余的字节全部设置为1, 即表示块图后后面的bit表示的块已经被占用 (实际上这些块并不存在)
    uint32_t last_size = SECTOR_SIZE - block_bitmap_last_byte % SECTOR_SIZE;
    memset(&buf[block_bitmap_last_byte], 0xFF, last_size);
    // 把被误杀的位设置回来
    uint8_t bit_idx = 0;
    while (bit_idx <= block_bitmap_last_bit)
        buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
    // 写入
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);

    // 4.3 写入inode位图
    memset(buf, 0, buf_size);
    buf[0] |= 0x1;           // 目前所有的inode中, 第0个inode被分配给根目录了
    // 我们文件系统中, 设置的inode位4096个, 因此其实只需要一个扇区就能够保存inode_bitmap
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);


    // 5. 初始化inode数组
    memset(buf, 0, buf_size);
    inode_t *i = (inode_t *)buf;
    // 5.1 先创建根目录的inode
    i->i_size = sb.dir_entry_size * 2;              // 根目录中有.和..
    i->i_no = 0;                                    // 根目录占用了0号inode
    i->i_sectors[0] = sb.data_start_lba;            // 根目录中直接存储数据开始区域的lba
    ide_write(hd, sb.inode_table_lba, buf, inode_table_sects);      // 前面memset, 剩下的buf都是0

    // 6. 根目录写入到sb.data_start_lba中的第一个块
    memset(buf, 0, buf_size);
    dir_entry_t *de = (dir_entry_t *)buf;

    memcpy(de->filename, ".", 1);                   // 根目录中第一个目录是.
    de->i_no = 0;                                   // 指向根目录自己
    de->f_type = FT_DIRECTORY;
    de++;
    memcpy(de->filename, "..", 2);                  // 根目录中第二个目录是..
    de->i_no = 0;                                   // 只想根目录自己
    de->f_type = FT_DIRECTORY;
    ide_write(hd, sb.data_start_lba, buf, 1);
    kprintf("    root_dir_lba: 0x%x\n", sb.data_start_lba);

    kprintf("Partition %s, format done\n", partition->name);
    kprintf("-------------------------------------------------------------------------------\n");
    sys_free(buf);
}



// 当前正在操作的分区
partition_t *current_partition;

/**
 * @brief mount_partition用于挂载分区, 即设置current_partition.
 *          ide.c中的ide_init函数在初始化硬盘的时候, 就已经扫描了硬盘中所有的分区, 并且将所有分区插入到partition_list的全局链表中
 *          因此, mount_partition函数用于在partition_list中进行扫描, 然后将current_partition设置为需要设置的分区
 *        需要注意的是, 在ide.c的partition_scan中只会计算记录partition的起始lba号等信息, 而诸如记录分区内具体得文件系统信息的
 *        super_block, 记录打开的inode的open_inode list, 记录已经分配出去的block的block_bitmap, 已经分配出的的inode的
 *        inode_bitmap等数据都没有从磁盘中读出来, 或者在内存中初始化
 *        所以, mount_partition除了设置current_partition以外, 还会完成上面说的这些内容, 即:
 *          1. 在内存中初始化partition.sb, 并从磁盘中读取该分区的super_block信息, 复制到partition.sb中
 *          2. 在内存中初始化partition.block_bitmap, 并从磁盘中读取该分区的block_bitmap信息, 复制到partition.block_bitmap中
 *          3. 在内存中初始化partition.inode_bitmap, 并从磁盘中读取该分区的inode_bitmap信息, 复制到partition.inode_bitmap中
 *          4. 在内存中初始化partition.open_inode list
 * 
 * @param elem 一开始的时候, 要将其设置为list_partition中的首个partition
 * @param arg 需要挂载的分区名
 * @return true 无意义, 用于给list_traversal停止遍历
 * @return false 无意义, 用于给list_traversal继续遍历
 */
bool mount_partition(list_elem_t *elem, int arg){
    char *partition_name = (char *)arg;
    partition_t *partition = elem2entry(partition_t, part_tag, elem);

    if (!strcmp(partition->name, partition_name)){
        current_partition = partition;
        disk_t *hd = current_partition->my_disk;

        // 虽然等下读取super_block可以一步到位读到current_partition->sb中
        // 但是后面还会用到读取的super_block中的信息, 所以这里保留一份备份
        super_block_t *sb_buf = (super_block_t *)sys_malloc(SECTOR_SIZE);
        
        // 1. 加载super_block
        // 1.1 分配存空间
        current_partition->sb = (super_block_t *)sys_malloc(sizeof(super_block_t));
        if (current_partition->sb == NULL)
            PANIC("mount_partition: sys_malloc for current_patitiion.sb fail!");

        // 1.2 读入超级块到io_buf中
        memset(sb_buf, 0, SECTOR_SIZE);
        ide_read(hd, current_partition->start_lba + 1, sb_buf, 1);
        // 1.3 复制io_buf中的数据到current_partition.sb中去
        memcpy(current_partition->sb, sb_buf, sizeof(super_block_t));

        // 2. 加载block_bitmap
        // 2.1 分配内存
        current_partition->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
        if (current_partition->block_bitmap.bits == NULL)
            PANIC("mount_partition: sys_malloc for current_partition->block_bitmap.bits fail!");
        current_partition->block_bitmap.btmp_byte_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;

        // 2.2 一步到位, 直接从磁盘中读取block bitmap到current_partition->block_bitmap.bits
        ide_read(hd, sb_buf->block_bitmap_lba, current_partition->block_bitmap.bits, sb_buf->block_bitmap_sects);


        // 3. 加载inode_bitmap
        // 3.1 分配内存
        current_partition->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
        if (current_partition->inode_bitmap.bits == NULL)
            PANIC("mount_partition: sys_malloc for current_partition->inode_bitmap.bits fail!");
        current_partition->inode_bitmap.btmp_byte_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;

        // 3.2 一步到位的读取
        ide_read(hd, sb_buf->inode_bitmap_lba, current_partition->inode_bitmap.bits, sb_buf->inode_bitmap_sects);

        // 初始化打开的inode链表
        list_init(&current_partition->open_inodes);
        kprintf("mount %s done!\n", partition->name);

        // 释放内存
        sys_free(sb_buf);

        // 停止遍历
        return true;
    }
    // 继续遍历
    return false;
}



// 在ide.c中声明
extern uint8_t channel_cnt;
extern ide_channel_t channels[8];          ///< 系统当前最大支持两个ide通道
extern list_t partition_list;              ///< 磁盘中分区表的链表, 在ide.c中的partition_scan中已经建立好了

// 在fs.h中声明
// extern file_desc_t *file_table;

/**
 * @brief filesys_init用于在磁盘上逐分区搜索文件系统, 如果磁盘上没有文件系统, 则创建一个文件系统
 */
void filesys_init(void){
    uint8_t channel_no = 0, dev_no = 0, partition_idx = 0;


    // 分配内存空间, 用于存储硬盘中读取出来的超级块
    super_block_t *sb_buf = (super_block_t*)sys_malloc(SECTOR_SIZE);
    if (sb_buf == NULL)
        PANIC("filesys_init: sb_buf malloc fail!");

    kprintf("Searching file system ......\n");
    
    while (channel_no < channel_cnt) {
        dev_no = 0;
        kprintf("=> Searching channel: %d\n", channel_no);
        while (dev_no < 2){
            // 跳过主盘, 主盘存放操作系统, raw disk
            if (dev_no == 0){
                dev_no++;
                continue;
            }
            // 依次检查从盘上的分区
            disk_t *hd = &channels[channel_no].devices[dev_no];
            kprintf("    => Searching disk: %s\n", hd->name);;

            partition_t *partition = hd->prim_parts;
            while (partition_idx < 12){             // 系统目前最大支持: 4个主分区 + 8个逻辑分区
                if (partition_idx == 4)
                    partition = hd->logic_parts;
                
                // 分区存在, 然后在处理分区
                if (partition->sec_cnt != 0){
                    // 每次读取前都要清0
                    memset(sb_buf, 0, SECTOR_SIZE);
                    // 读取分区的超级块
                    ide_read(hd, partition->start_lba + 1, sb_buf, 1);

                    // 判断是不是JackOS的文件系统, 如果不是则格式化为JackOS的文件系统
                    if (sb_buf->magic == 0x20010107)
                        kprintf("        => Partition %s: fs detected\n", partition->name);
                    else {
                        kprintf("        => Partition %s: fs not detected\n", partition->name);
                        kprintf("            => formatting...\n");
                        partition_format(partition);
                    }
                }
                // 处理下一个分区
                partition++;
                partition_idx++;
            }
            // 处理下一个硬盘(device)
            dev_no++;
        }
        // 处理下一个通道
        channel_no++;
    }
    sys_free(sb_buf);

    // 设置默认分区
    char default_part[8] = "sdb1";
    list_traversal(&partition_list, mount_partition, (int)default_part);

    // 打开默认分区上的根目录
    open_root_dir(current_partition);

    // 初始化文件列表
    uint32_t fd_idx = 0;
    while (fd_idx < MAX_FILE_OPEN){
        file_table[fd_idx].fd_pos = 0;
        // file_table[fd_idx].fd_flag = 0;
        file_table[fd_idx].fd_inode = NULL;
        fd_idx++;
    }
}



/**
 * @brief path_parse用于获得文件路径pathname的顶层路径, 顶层路径存入到name_store中
 *        举一个例子如下: 
 *          pathname="/home/jack", char name_store[10]
 *          path_parse(pathname, name_store) -> return "/jack", *name_store="home"
 * 
 * @example pathname="/home/jack", char name_store[10]
 *          path_parse(pathname, name_store) -> return "/jack", *name_store="home"
 * 
 * @param pathname 需要解析的文件路径
 * @param name_store pathname中的顶层路径
 * @return char* 指向路径名中下一个名字
 */
char *path_parse(char *pathname, char *name_store){
    // 根目录不需要解析, 跳过即可
    if (pathname[0] == '/')
        while (*(++pathname) == '/');           // 跳过'//a', '///b'

    while (*pathname != '/' && *pathname != 0)  // 一直复制顶层目录结束, 例如将abc/xxxx中的abc复制到name_store中
        *name_store++ = *pathname++;
    
    if (pathname[0] == 0)                       // pathname为空, 则表示路径已经结束了, 此时返回NULL
        return NULL;
    
    return pathname;
}


/**
 * @brief path_depth_cnt用于返回路径pathname的深度. 注意, 所谓路径的深度是指到文件的深度, 例如: /a的深度是1, /a/b的深度
 *        是2, /a/b/c/d/e的深度是5
 * 
 * @param pathname 需要判断深度的路径名
 * @return uint32_t 路径的深度
 */
uint32_t path_depth_cnt(char* pathname){
    ASSERT(pathname != NULL);

    char *p = pathname;
    char name[MAX_FILE_NAME_LEN];
    uint32_t depth = 0;
    p = path_parse(p, name);
    while (name[0]){
        depth++;
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (p)
            p = path_parse(p, name);
    }
    return depth;
}


/**
 * @brief search_file用于搜索给定的文件. 若能找到, 则返回要搜索的文件的inode号, 若找不到则返回-1
 * 
 * @param pathname 要搜索的文件的绝对路径
 * @param searched_record 寻找的记录
 * @return int 若成功, 则返回文件的inode号, 若找不到则返回-1
 */
int search_file(const char *pathname, path_search_record *searched_record){
    // 如果要找的目录是根目录, 则填充信息后直接返回
    if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")){
        searched_record->searched_path[0] = 0;
        searched_record->parent_dir = &root_dir;
        searched_record->file_type = FT_DIRECTORY;
        // 根目录是0好inode
        return 0;
    }

    uint32_t path_len = strlen(pathname);
    // 路径必须是绝对路径, 所以至少是'/a'这样的形式, 并且需要少于最长路径长
    ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
    char *sub_path = (char*) pathname;
    dir_t *parent_dir = &root_dir;
    dir_entry_t dir_e;

    // 等会循环解析, 不断去除顶层目录, 而后每次去除的顶层目录存储在name中
    char name[MAX_FILE_NAME_LEN] = {0};

    // parent dir初始化的时候就是UNKNOW
    searched_record->file_type = FT_UNKNOW;
    searched_record->parent_dir = parent_dir;
    uint32_t parent_inode_no = 0;

    // 第一次解析
    sub_path = path_parse(sub_path, name);
    while (name[0]){
        // 文件路径必须小于最大长度
        ASSERT(strlen(searched_record->searched_path) < MAX_PATH_LEN);

        // 记录已经存在的所以父集目录
        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name);

        // 在给定的目录中寻找文件
        if (search_dir_entry(current_partition, parent_dir, name, &dir_e)){
            memset(name, 0, MAX_FILE_NAME_LEN);
            if (sub_path)
                sub_path = path_parse(sub_path, name);          // 如果解析后的目录还不是空的, 那么继续的解析, 即下一次while
            
            
            if (FT_DIRECTORY == dir_e.f_type){
                // 如果接下来要打开的是目录, 那么就表示还没有找到最终的目标文件
                // 那么就先关闭之前打开的目录, 然后再打开接下来要在其中查找的文件
                parent_inode_no = parent_dir->inode->i_no;
                // 先关闭之前的parent_dir, 然后再打开新找到的文件夹
                dir_close(parent_dir);
                parent_dir = dir_open(current_partition, dir_e.i_no);
                searched_record->parent_dir = parent_dir;
                continue;
            } else if (FT_REGULAR == dir_e.f_type){
                // 如果是普通文件, 那么就找到头了, 此时返回该文件的inode即可
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no;
            }
        } else
            // 表示parent_dir中没有名字为name的文件, 直接返回-1即可
            // 但是不要关闭parent_dir, 因为可能调用search_file的函数根据 path_search_record来判断
            // 发现要查找的文件不存在, 那么就会创建, 因此不要不关闭parent_dir
            return -1;
    }

    // 执行到此, 必然是:
    //      1. 遍历完了完整路径, 即从while(name[0])出来
    //      2. 最后的目录中只存在要查找的文件的或者目录的同名的目录, 即从if (FT_DIRECTORY == dir_e.ftype)中出来
    // 因此, 如果我们最后要查找的就是目录, 那么就没问题, 如果要找的是文件的, 那么就有问题
    // 但是, 因为我们只是负责找到这个名字的文件, 具体是文本文件还是目录文件不管, 该怎么处理交给主调函数来决定最后干嘛

    // 此时因为continue -> while(name[0])退出循环前, 有一个dir_open, 所以得关闭这个目录, 然后重新打开父目录
    dir_close(searched_record->parent_dir);
    searched_record->parent_dir = dir_open(current_partition, parent_inode_no);
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;
}




/**
 * @brief sys_open是open系统调用的实现函数, 用于打开一个指定的文件, 如果文件不存在的话, 则会创建文件, 而后打开该文件
 * 
 * @param pathname 需要打开的文件的绝对路径
 * @param flags 需要打开的文件的读写标志
 * @return int32_t 若成功打开文件, 则返回线程tcb中的文件描述符, 若失败, 则返回-1
 */
int32_t sys_open(const char *pathname, uint8_t flags){
    // 目前不支持打开目录, 只支持打开文件, 所以如果打开的是目录, 就报错
    if (pathname[strlen(pathname) - 1] == '/'){
        kprintf("sys_open: cannot open a directory %s\n", pathname);
        return -1;
    }
    ASSERT(flags <= 7);

    // 默认是找不到一个system-wide file descriptor
    int32_t fd = -1;
    path_search_record searched_record;
    // 一般来说, 初始化了变量之后为了避免遗留, 所以需要为其赋初值. 这里因为是一个结构体, 所以要给结构体占用的内存清0
    memset(&searched_record, 0, sizeof(path_search_record));

    uint32_t pathname_depth = path_depth_cnt((char*)pathname);
    int inode_no = search_file(pathname, &searched_record);
    bool found = inode_no == -1 ? false : true;

    // seach_file不会对同名目录进行处理, 而目前不支持打开目录, 所以需要提示
    if (searched_record.file_type == FT_DIRECTORY){
        kprintf("sys_open: cannot open a directory with open(), use opendir() instead!\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }
    
    // 前面有一个问题, 就是如果给出的目录是/a/b/c/d, 而目前只存在/a/b/, c不存在
    // 所以此时还需要检查一下文件路径游走的时候, 是否访问到了/a/b/c, 即检查已经访问的路径是否是c的父目录
    uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
    if (pathname_depth != path_searched_depth){
        kprintf("Cannot access %s: Not a directory, subpath %s doesn't exist\n", pathname, searched_record.searched_path);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // 如果文件不存在, 并且文件的属性不存在O_CREAT, 则报错, 而后输出-1
    if (!found && !(flags & O_CREAT)){
        kprintf(
            "sys_open: file %s doesn't exists in path %s\n",
            strrchr(searched_record.searched_path, '/') + 1,
            searched_record.searched_path
        );
        dir_close(searched_record.parent_dir);
        return -1;
    } else if (found && flags & O_CREAT) {
        // 如果文件已经存在, 并且还要创建, 则报错
        kprintf("%s has already exists!\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }


    switch (flags & O_CREAT){
        // 若文件有O_CREAT属性, 则创建文件
        case O_CREAT:
            kprintf("Creating file: %s\n", pathname);
            fd = file_create(
                searched_record.parent_dir,
                strrchr(pathname, '/') + 1,
                flags
            );
            dir_close(searched_record.parent_dir);
            break;
        // 其他情况打开文件即可
        default:
            fd = file_open(inode_no, flags);
            break;
    }

    return fd;
}


/**
 * @brief fd_local2global用于将进程的局部文件描述符
 * 
 * @param local_fd 需要转换的局部文件描述符
 * @return uint32_t 转换后的全局文件描述符
 */
uint32_t fd_local2global(uint32_t local_fd){
    task_struct_t *cur = running_thread();
    int32_t global_fd = cur->fd_table[local_fd];
    ASSERT(0 <= global_fd && global_fd < MAX_FILE_OPEN_PER_PROC);
    return (uint32_t)global_fd;
}


/**
 * @brief sys_close用于关闭文件描述符fd指向的文件.
 * 
 * @param fd 
 * @return int32_t 若关闭成功, 则返回0; 若关闭失败, 则返回-1
 */
int32_t sys_close(int32_t fd){
    int32_t ret = -1;
    if (fd > 2){
        uint32_t fd_global = fd_local2global(fd);
        ret = file_close(&file_table[fd_global]);
        running_thread()->fd_table[fd] = -1;
    }
    return ret;
}

/**
 * @brief sys_write是write系统调用的实现函数. 用于向fd指定的文件中写入buf中count个字节的数据
 * 
 * @param fd 需要写入的文件描述符
 * @param buf 需要写入的数据所在的缓冲区
 * @param count 需要写入的字节数
 * 
 * @return int32_t 若写入成功, 则返回写入的字节数; 若写入失败, 则返回 -1
 */
int32_t sys_write(int32_t fd, const void *buf, uint32_t count){
    if (fd < 0){
        kprintf("sys_write: fd error\n");
        return -1;
    }

    // stdout则输出到屏幕中
    if (fd == stdout_no){
        char temp_buf[1024] = {0};
        memcpy(temp_buf, buf, count);
        console_put_str(temp_buf);
        return count;
    }

    uint32_t _fd = fd_local2global(fd);
    file_desc_t *wr_file = &file_table[_fd];
    if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWD){
        uint32_t bytes_written = file_write(wr_file, buf, count);
        return bytes_written;
    } else {
        console_put_str("sys_write: not allowed to write file without writing permission: O_RDWR or O_WRONLY not found\n");
        return -1;
    }
}

extern ioqueue_t kbd_buf;

/**
 * @brief sys_read是read系统的调用的实现函数. 用于从fd指向的文件中读取count个字节到buf中
 *
 * @param fd 需要读取的文件描述符
 * @param buf 读取的数据将写入的缓冲区
 * @param count 需要读取的字节数
 * @return int32_t 若读取成功, 则返回读取的字节数; 若读取失败, 则返回 -1
 */
int32_t sys_read(int32_t fd, void *buf, uint32_t count){
    ASSERT(buf != NULL);
    int ret = -1;
    if (fd < 0 || fd == stdout_no || fd == stderr_no){
        kprintf("%s: fd error\n", __func__);
        return -1;
    } else if (fd == stdin_no){
        char *buffer = buf;
        uint32_t byte_read = 0;
        while (byte_read < count){
            *buffer = ioq_getchar(&kbd_buf);
            byte_read++;
            buffer++;
        }
        ret = (byte_read == 0 ? -1 : (int32_t)byte_read);
    } else {
        uint32_t _fd = fd_local2global(fd);
        ret = file_read(&file_table[_fd], buf, count);
    }
    return ret;
}



/**
 * @brief sys_lseek是lseek系统调用的实现函数. 用于设置fd指向的文件的读写指针
 * 
 * @param fd 需要设置读写指针的文件
 * @param offset 要设置的偏移量
 * @param whence 设置新的偏移量的方式. 其中:
 *          1. SEEK_SET 会将文件新的读写位置设置为相对文件开头的offset字节处, 此时要求offset为正值
 *          2. SEEK_CUR 会将文件新的读写位置设置为相对当前位置的offset字节处, 此时要求offset正负均可
 *          3. SEEK_END 会将文件新的读写位置设置为相对文件结尾的offset字节处, 此时要求offset为负值
 * @return int32_t 若设置成功, 则返回新的偏移量; 若设置失败, 则返回-1
 */
int32_t sys_lseek(int32_t fd, int32_t offset, whence_t whence){
    // check fd
    if (fd < 0){
        kprintf("sys_lseek: fd_error\n");
        return -1;
    }
    // check whence
    ASSERT(whence > 0 && whence < 4);


    uint32_t global_fd = fd_local2global(fd);
    file_desc_t *pf = &file_table[global_fd];

    int32_t new_pos = 0;
    int32_t file_size = (int32_t) pf->fd_inode->i_size;
    switch (whence){
        // 1. SEEK_SET 会将文件新的读写位置设置为相对文件开头的offset字节处
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = (int32_t)pf->fd_pos + offset;
            break;
        case SEEK_END:
            new_pos = file_size + offset;
            break;
    }

    if (new_pos < 0 || new_pos > (file_size - 1)){
        return -1;
    }
    pf->fd_pos = new_pos;
    return pf->fd_pos;
}


/**
 * @brief sys_unlink是unlink系统调用的实现函数, 用于删除文件(非目录)
 * 
 * @param pathname 需要删除的文件路径名
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t sys_unlink(const char* pathname){
    ASSERT(strlen(pathname) < MAX_PATH_LEN);

    // 先查找需要删除的文件是否存在
    path_search_record searched_record;
    memset(&searched_record, 0, sizeof(path_search_record));
    int inode_no = search_file(pathname, &searched_record);

    ASSERT(inode_no != 0);          // inode_no == 0 是根目录的

    // 文件不存在
    if (inode_no == -1){
        kprintf("%s: %s not found!\n", __func__, pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // 目前不支持删除目录, 所以报个错
    if (searched_record.file_type == FT_DIRECTORY){
        kprintf("%s: cannot delete a directory with %s, use sys_rmdir() instead\n", __func__, __func__);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // 检查文件是否在已经打开的文件列表中
    uint32_t file_idx = 0;
    while (file_idx < MAX_FILE_OPEN){
        if (file_table[file_idx].fd_inode != NULL
                &&
            (uint32_t)inode_no == file_table[file_idx].fd_inode->i_no)
            break;
        file_idx++;
    }

    // 文件如果在已经打开的文件列表中, 则需要首先关闭文件
    if (file_idx < MAX_FILE_OPEN){
        dir_close(searched_record.parent_dir);
        kprintf("%s: file %s is being using now, cannot delete!\n", __func__, pathname);
        return -1;
    }
    ASSERT(file_idx == MAX_FILE_OPEN);

    // 回收block bitmap, block后面会被覆盖


    // 删除目录项, 后面会覆盖
    void *io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (io_buf == NULL){
        dir_close(searched_record.parent_dir);
        kprintf("%s: sys_malloc for io_buf failed!\n", __func__);
        return -1;
    }


    dir_t* parent_dir = searched_record.parent_dir;
    delete_dir_entry(current_partition, parent_dir, inode_no, io_buf);
    inode_release(current_partition, inode_no);
    sys_free(io_buf);
    dir_close(parent_dir);

    return 0;
}


/**
 * @brief sys_mkdir是mkdir系统调用的实现函数. 用于在磁盘上创建路径名为pathname的文件
 * 
 * @param pathname 需要创建的目录的路径名
 * @return int32_t 若创建成功, 则返回0; 若创建失败, 则返回-1
 */
int32_t sys_mkdir(const char* pathname){
    uint8_t rollback_step = 0;

    // 1. 首先分配io_buf, 用于稍后读写硬盘用
    void *io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (io_buf == NULL){
        kprintf("%s: sys_malloc for io_buf failed\n", __func__, pathname);
        return -1;
    }

    // 2. 首先搜索一下, 确保不存在同名的文件或者目录
    path_search_record searched_record;
    memset(&searched_record, 0, sizeof(path_search_record));
    int inode_no = -1;
    inode_no = search_file(pathname, &searched_record);
    // 2.1 确保不存在同名文件或者目录
    if (inode_no != -1){
        kprintf("%s: %s exists!\n", __func__, pathname);
        rollback_step = 1;
        goto rollback;
    // 2.2 不存在还得判断下是目录本身不存在还是目录的父目录不存在
    } else {
        // 利用文件的深度和已经搜索的路径的深度来判断是否已经全部遍历完
        uint32_t pathname_depth = path_depth_cnt((char*) pathname);
        uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
        // 深度不等, 说明中间某个文件目录不存在
        if (pathname_depth != path_searched_depth){
            kprintf("sys_mkdir: cannot access %s, parent directory %s not exists!\n", pathname, searched_record.searched_path);
            rollback_step = 1;          // 因为search_path失败了, 所以得回滚去关闭searched_path.parent_dir
            goto rollback;
        }
    }


    // pathname可能有'/', 因此使用不含'/'的searched_record
    dir_t* parent_dir = searched_record.parent_dir;
    char *dirname = strrchr(searched_record.searched_path, '/') + 1;

    // 为目录分配inode
    inode_no = inode_bitmap_alloc(current_partition);
    if (inode_no == -1){
        kprintf("%s: alllocate inode for directory failed!\n", __func__);
        rollback_step = 1;              // inode分配失败, 回滚去关闭searched_path.parent_dir
        goto rollback;
    }
    // 初始化inode
    inode_t new_dir_inode;
    inode_init(inode_no, &new_dir_inode);


    // 分配第一个块
    uint32_t block_bitmap_idx = 0;
    int32_t block_lba = block_bitmap_alloc(current_partition);
    if (block_lba == -1){
        kprintf("%s: block_bitmap_alloc failed!\n", __func__);
        rollback_step = 2;              // 分配块失败, 除了关闭parent_dir, 还得释放inode
        goto rollback;
    }
    new_dir_inode.i_sectors[0] = block_lba;
    block_bitmap_idx = block_lba - current_partition->sb->data_start_lba;
    ASSERT(block_bitmap_idx != 0);
    // 分配完了之后就同步到磁盘
    bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);

    
    // 写入'.'和'..'到磁盘
    memset(io_buf, 0, SECTOR_SIZE * 2);
    dir_entry_t *de = (dir_entry_t *) io_buf;
    // 初始化'.'
    memcpy(de->filename, ".", 1);
    de->i_no = inode_no;
    de->f_type = FT_DIRECTORY;
    de++;
    // 初始化'..'
    memcpy(de->filename, "..", 2);
    de->i_no = parent_dir->inode->i_no;
    de->f_type = FT_DIRECTORY;
    // 修改inode大小
    new_dir_inode.i_size = 2 * current_partition->sb->dir_entry_size;
    // 写入inode的数据块中
    ide_write(current_partition->my_disk, new_dir_inode.i_sectors[0], io_buf, 1);


    // 在父目录中添加自己
    dir_entry_t new_de;
    memset(&new_de, 0, sizeof(dir_entry_t));
    create_dir_entry(dirname, inode_no, FT_DIRECTORY, &new_de);
    memset(io_buf, 0, SECTOR_SIZE * 2);
    if (!sync_dir_entry(parent_dir, &new_de, io_buf)){
        kprintf("%s: sync_dir_entry to disk failed!\n", __func__);
        rollback_step = 2;              // 同上, 关闭parent_dir还得释放inode
        goto rollback;
    }
    // 同步父目录的inode
    memset(io_buf, 0, SECTOR_SIZE * 2);
    inode_sync(current_partition, parent_dir->inode, io_buf);

    // 将新创建的inode同步到磁盘
    memset(io_buf, 0, SECTOR_SIZE * 2);
    inode_sync(current_partition, &new_dir_inode, io_buf);

    // 清除局部资源
    sys_free(io_buf);
    dir_close(searched_record.parent_dir);

    return 0;

rollback:
    switch (rollback_step){
        case 2:
            bitmap_set(&current_partition->inode_bitmap, inode_no, 0);
            __attribute__ ((fallthrough));
        case 1:
            dir_close(searched_record.parent_dir);
            break;
    }
    sys_free(io_buf);
    return -1;
}


/**
 * @brief sys_opendir是opendir系统调用的实现函数. 用于打开目录名为pathname的目录
 * 
 * @param pathname 需要打开目录的路径名
 * @return dir_t* 若打开成功, 则返回目录指针; 若打开失败则返回NULL
 */
dir_t* sys_opendir(const char* pathname){
    ASSERT(strlen(pathname) < MAX_PATH_LEN);

    dir_t *ret = NULL;

    // 根目录直接返回
    if (pathname[0] == '/' && (pathname[1] == 0 || pathname[0] == '.'))
        return &root_dir;

    // 检查要打开的目录是否存在, 通过搜索的方式实现
    path_search_record searched_record;
    memset(&searched_record, 0, sizeof(path_search_record));
    int inode_no = search_file(pathname, &searched_record);
    // 目录不存在, 输出信息然后返回
    if (inode_no == -1){
        kprintf("%s: In %s, sub path not exists\n", __func__, pathname, searched_record.searched_path);
    } else {
        if (searched_record.file_type == FT_REGULAR)
            kprintf("%s: %s is regular file\n", __func__, pathname);
        else if (searched_record.file_type == FT_DIRECTORY) 
            ret = dir_open(current_partition, inode_no);
    }
    dir_close(searched_record.parent_dir);
    return ret;
}


/**
 * @brief sys_closedir是closedir系统调用的实现函数. 用于关闭一个文件夹
 * 
 * @param dir 需要关闭的文件夹
 * @return int32_t 若关闭成功, 则返回0; 若关闭失败, 则返回-1
 */
int32_t sys_closedir(dir_t *dir){
    int32_t ret = -1;
    if (dir != NULL){
        dir_close(dir);
        ret = 0;
    }
    return ret;
}


/**
 * @brief sys_readdir是readdir系统调用的实现函数. 用于读取dir指向的目录中的目录项
 * 
 * @param dir 指向需要读取的目录的指针
 * @return dir_entry_t* 若读取成功, 则返回指向目录项的指针; 若读取失败, 则返回NULL
 */
dir_entry_t *sys_readdir(dir_t *dir){
    ASSERT(dir != NULL);
    return dir_read(dir);
}


/**
 * @brief sys_rewinddir是rewinddir系统调用的实现函数. 用于将dir指向的目录中的dir_pose重置为0
 * 
 * @param dir 指向需要重置的文件夹
 */
void sys_rewinddir(dir_t *dir){
    dir->dir_pos = 0;
}


/**
 * @brief sys_rmdir是rmdir系统调用的实现函数. 用于删除空目录
 * 
 * @param pathname 需要删除的空目录的路径名
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t sys_rmdir(const char* pathname){
    path_search_record searched_record;
    memset(&searched_record, 0, sizeof(path_search_record));

    // 删除目录前一定要确保能够找到目录
    int inode_no = search_file(pathname, &searched_record);
    ASSERT(inode_no != 0);

    int32_t retval = -1;
    if (inode_no == -1){
        kprintf("%s: In %s, subpath %s not exists!\n", __func__, pathname, searched_record.searched_path);
    } else {
        if (searched_record.file_type == FT_REGULAR){
            kprintf("%s: %s is regular file!\n", __func__, pathname);
        } else {
            dir_t *dir = dir_open(current_partition, inode_no);
            if (!dir_is_empty(dir)){
                kprintf("dir %s is not empty, not allowed to remove non-empty directory!\n", pathname);
            } else {
                if (!dir_remove(searched_record.parent_dir, dir)){
                    retval = 0;
                }
            }
            dir_close(dir);
        }
    }
    dir_close(searched_record.parent_dir);
    return retval;
}


/**
 * @brief get_parent_dir_inode_nr用于获得获取目录所在的父目录的inode编号. 
 *        原理就是子目录中的'..'这一项存储了父目录的inode号, 所以读取子目录的内容,
 *        然后返回其中文件名为'..'的这个目录项的inode号即可
 * 
 * @param child_inode_no 文件的inode号
 * @param io_buf 由调用者提供的io_buf, 用于读写硬盘用
 * @return uint32_t 文件所在的父目录的inode编号
 */
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_no, void* io_buf){
    // 获取子目录的block号
    inode_t *child_dir_inode = inode_open(current_partition, child_inode_no);
    uint32_t block_lba = child_dir_inode->i_sectors[0];
    ASSERT(block_lba >= current_partition->sb->data_start_lba);
    inode_close(child_dir_inode);

    // 读取子目录的目录项信息
    ide_read(current_partition->my_disk, block_lba, io_buf, 1);
    dir_entry_t *de = (dir_entry_t *)io_buf;

    // 创建文件的事后, 第0个目录项是'.', 第1个目录项是'..'
    ASSERT(de[1].i_no < 4096 && de[1].f_type == FT_DIRECTORY);
    return de[1].i_no;
}


/**
 * @brief get_child_dir_name用于在编号为p_inode_no的目录中循找inode编号为c_inode_no的子目录的名称,
 *        名称将存入path中
 * 
 * @param p_inode_no 要搜索的父目录的inode编号
 * @param c_inode_no 要寻找的子目录的inode编号
 * @param path inode编号为c_inode_no的子目录名称
 * @param io_buf 调用者提供的读取硬盘时候的io_buf
 * @return int32_t 若读取成功则返回0, 失败则返回-1
 */
static int32_t get_child_dir_name(uint32_t p_inode_no, uint32_t c_inode_no, char *path, void *io_buf){

    // 1. 读取父目录中所有的数据, 然后一次比较即可
    inode_t *parent_dir_inode = inode_open(current_partition, p_inode_no);
    // 先读取所有的块lba信息
    uint8_t block_idx = 0, block_cnt = 12;
    uint32_t all_blocks_lba[140] = {0};
    // 直接块
    while (block_idx < 12){
        all_blocks_lba[block_idx] = parent_dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    // 一级间接块
    if (parent_dir_inode->i_sectors[12] != 0){
        ide_read(current_partition->my_disk, parent_dir_inode->i_sectors[12], all_blocks_lba + 12, 1);
        block_cnt = 140;
    }
    inode_close(parent_dir_inode);

    // 遍历所有块来寻找子目录
    dir_entry_t *de = (dir_entry_t *)io_buf;
    uint32_t dir_entry_size = current_partition->sb->dir_entry_size;
    uint32_t dir_entry_per_sec = SECTOR_SIZE / dir_entry_size;
    block_idx = 0;
    while (block_idx < block_cnt){
        // 档当前块不为空, 则读入该块
        if (all_blocks_lba[block_idx]){
            ide_read(current_partition->my_disk, all_blocks_lba[block_idx], io_buf, 1);
            // 逐个遍历目录项
            uint32_t de_idx = 0;
            while (de_idx < dir_entry_per_sec){
                if ((de + de_idx)->i_no == c_inode_no){
                    strcat(path, "/");
                    strcat(path, (de + de_idx)->filename);
                    return 0;
                }
                de_idx++;
            }
        }
        // 下一个块
        block_idx++;
    }
    // 没有找到
    return -1;
}



/**
 * @brief sys_getcwd用于将当前运行线程的工作目录的绝对路径写入到buf中
 * 
 * @param buf 由调用者提供存储工作目录路径的缓冲区, 若为NULL则将由操作系统进行分配
 * @param size 若调用者提供buf, 则size为buf的大小
 * @return char* 若成功且buf为NULL, 则操作系统会分配存储工作目录路径的缓冲区, 并返回首地址; 若失败则为NULL
 */
char *sys_getcwd(char *buf, uint32_t size){
    ASSERT(buf != NULL);
    void *io_buf = sys_malloc(SECTOR_SIZE);
    if (io_buf == NULL) 
        return NULL;

    task_struct_t *cur = running_thread();
    int32_t parent_inode_no = 0;
    int32_t child_inode_no = cur->cwd_inode_no;
    ASSERT(0 <= child_inode_no && child_inode_no < 4096);

    // 当前工作目录是根目录
    if (child_inode_no == 0){
        buf[0] = '/';
        buf[1] = 0;
        return buf;
    }

    memset(buf, 0, size);
    char full_path_reverse[MAX_PATH_LEN] = {0};

    // 从子目录开始逐层向上找, 一直找到根目录为止, 每次查找都会把当前的目录名复制到full_path_reverse中
    // 例如子目录现在是"/fd1/fd1.1/fd1.1.1/fd1.1.1.1", 
    // 则运行结束之后, full_path_reverse为 "/fd1.1.1.1/fd1.1.1/fd1.1/fd1"
    while (child_inode_no){
        parent_inode_no  = get_parent_dir_inode_nr(child_inode_no, io_buf);
        // 如果没有找到就报错
        if (get_child_dir_name(parent_inode_no, child_inode_no, full_path_reverse, io_buf) == -1){
            sys_free(io_buf);
            return NULL;
        }
        child_inode_no = parent_inode_no;
    }
    ASSERT(strlen(full_path_reverse) < size);

    // 将反过来的目录名转正
    char *last_slash;
    // 只要没有找到"/fd1.1.1.1/fd1.1.1/fd1.1/fd1/"的第一个"/"
    while ((last_slash = strrchr(full_path_reverse, '/'))){
        uint16_t len = strlen(buf);
        // 把/fd1.1复制到, /fd1的后面
        strcpy(buf + len, last_slash);
        // 最后一位设置为0, 这样就是下一次strrchr就从这里开始查起
        *last_slash = 0;
    }
    sys_free(io_buf);

    return buf;
}


/**
 * @brief sys_chdir是chdir系统调用的实现函数. 用于更到当前线程的工作目录为path
 * 
 * @param path 将要修改的工作目录的绝对路径
 * @return int32_t 若修改成功则返回0; 若修改失败则返回-1
 */
int32_t sys_chdir(const char* path){
    int32_t ret = -1;
    path_search_record searched_record;
    memset(&searched_record, 0, sizeof(path_search_record));

    int inode_no = search_file(path, &searched_record);
    if (inode_no != -1){
        if (searched_record.file_type == FT_DIRECTORY){
            running_thread()->cwd_inode_no = inode_no;
            ret = 0;
        } else {
            kprintf("%s: %s is not directory!\n", __func__, path);
        }
    }
    dir_close(searched_record.parent_dir);
    return ret;
}


/**
 * @brief sys_stat用于查询path表示的文件的属性, 并将其填入到buf中
 * 
 * @param path 需要查询属性的文件路径
 * @param buf 由调用者提供的文件属性的缓冲区
 * @return int32_t 若运行成功, 则返回0; 若运行失败, 则返回-1
 */
int32_t sys_stat(const char* path, stat_t *buf){
    // 查询根目录
    if (!strcmp(path, "/") || !strcmp(path, "/.") || !strcmp(path, "/..")){
        buf->st_filetype = FT_DIRECTORY;
        buf->st_ino = 0;
        buf->st_size = root_dir.inode->i_size;
    }

    int32_t ret = -1;
    path_search_record searched_record;
    memset(&searched_record, 0, sizeof(path_search_record));

    int32_t inode_no = search_file(path, &searched_record);
    if (inode_no != -1){
        inode_t *obj_inode = inode_open(current_partition, inode_no);
        buf->st_size = obj_inode->i_size;
        inode_close(obj_inode);
        buf->st_filetype = searched_record.file_type;
        buf->st_ino = inode_no;
        ret = 0;
    } else 
        kprintf("%s: %s not found!\n", __func__, path);

    dir_close(searched_record.parent_dir);
    return ret;
}