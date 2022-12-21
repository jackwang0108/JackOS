#include "fs.h"
#include "ide.h"
#include "file.h"
#include "debug.h"
#include "string.h"
#include "kstdio.h"
#include "thread.h"
#include "interrupt.h"

file_desc_t file_table[MAX_FILE_OPEN];


/**
 * @brief get_free_slot_in_global用于从全局文件描述符表中找到一个空位
 * 
 * @return int32_t 若全局文件描述符表中有空闲的文件描述符, 则返回空闲的文件描述符表的索引; 
 *          若无空闲的文件描述符, 则返回-1
 */
int32_t get_free_slot_in_global(void){
    uint32_t fd_idx = 3;
    while (fd_idx < MAX_FILE_OPEN){
        if (file_table[fd_idx].fd_inode == NULL)
            break;
        fd_idx++;
    }
    
    if (fd_idx == MAX_FILE_OPEN){
        kprintf("exceed system max open files!\n");
        return -1;
    }
    return fd_idx;
}


/**
 * @brief pcb_fd_install用于将全局文件描述符索引安装到用户进程的文件描述符表中
 * 
 * @param globa_fd_idx 要安装的全局文件描述符表
 * @return int32_t 若成功安装到用户文件描述符表中, 则返回用户文件描述符表的索引, 若失败则返回-1
 */
int32_t pcb_fd_install(int32_t globa_fd_idx){
    task_struct_t *cur = running_thread();

    uint8_t local_fd_idx = 3;
    while (local_fd_idx < MAX_FILE_OPEN_PER_PROC){
        if (cur->fd_table[local_fd_idx] == -1){
            cur->fd_table[local_fd_idx] = globa_fd_idx;
            break;
        }
        local_fd_idx++;
    }

    if (local_fd_idx == MAX_FILE_OPEN_PER_PROC){
        kprintf("exceed thread max open files!\n");
        return -1;
    }

    return local_fd_idx;
}


/**
 * @brief inode_bitmap_alloc用于从partition的inode列表中分配一个inode
 * 
 * @param partition 需要分配inode的分区
 * @return int32_t 若分配成功, 则返回分配的得到的inode的index; 若分配失败, 则返回-1
 */
int32_t inode_bitmap_alloc(partition_t *partition){
    int32_t bit_idx = bitmap_scan(&partition->inode_bitmap, 1);
    if (bit_idx == -1)
        return -1;
    bitmap_set(&partition->inode_bitmap, bit_idx, 1);
    return bit_idx;
}


/**
 * @brief block_bitmap_alloc用于从partition指向的分区中分配一个block. 注意, 该函数只会修改内存中的block_bitmap,
 *        而不会修改物理磁盘中partition中的block bitmap
 * 
 * @param partition 需要分配block的分区
 * @return int32_t 若分配成功, 则返回分配的得到的block的lba扇区号; 若分配失败, 则返回-1
 */
int32_t block_bitmap_alloc(partition_t *partition){
    int32_t bit_idx = bitmap_scan(&partition->block_bitmap, 1);
    if (bit_idx == -1)
        return -1;
    bitmap_set(&partition->block_bitmap, bit_idx, 1);
    return partition->sb->data_start_lba + bit_idx;
}


/**
 * @brief bitmap_sync用于将内存中的bitmap的bit_idx位的值同步到partition指向的分区的中的inode bitmap或者block bitmap
 * 
 * @param partition 要写入的bitmap所在的分区
 * @param bit_idx 要写入的位
 * @param btmp 要写入的位图
 */
void bitmap_sync(partition_t *partition, uint32_t bit_idx, uint8_t btmp){
    // off_sec是要写入的位(bit_idx)相对于parition->sb->block_bitmap_lba的扇区偏移数
    uint32_t off_sec = bit_idx / 4096;
    // off_size是要写入的位(bit_idx)相对于parition->block_bitmap.bits或者partition->inode_bitmap.bits的字节偏移数
    // 因此, off_size是用于计算需要将内存中的哪个扇区写入到磁盘中用的
    uint32_t off_size = off_sec * BLOCK_SIZE;

    uint32_t sec_lba;
    uint8_t *bitmap_off;
    switch (btmp) {
        case INODE_BITMAP:
            sec_lba = partition->sb->inode_bitmap_lba + off_sec;
            bitmap_off = partition->inode_bitmap.bits + off_size;
            break;
        case BLOCK_BITMAP:
            sec_lba = partition->sb->block_bitmap_lba + off_sec;
            bitmap_off = partition->block_bitmap.bits + off_size;
            break;
        default:
            PANIC("Invalid bitmap type");
            break;
    }
    ide_write(partition->my_disk, sec_lba, bitmap_off, 1);
}

// 在fs.c中定义
extern partition_t *current_partition;

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
 * @param flag 需要创建的文件的读写属性
 * @return int32_t 若成功, 则返回文件描述符; 若失败, 则返回-1
 */
int32_t file_create(dir_t *parent_dir, char *filename, uint8_t flag){
    // 创建一个缓冲区, 用于稍后和磁盘的io
    void *io_buf;
    if ((io_buf = sys_malloc(1024)) == NULL){
        kprintf("file_create: io_buf create fail!\n");
        return -1;
    }

    int32_t inode_no = inode_bitmap_alloc(current_partition);
    if (inode_no == -1){
        kprintf("file_create: inode_bitmap_alloc failed!\n");
        return -1;
    }

    // 因为上面已经申请到了inode_bitmap这个资源, 所以此后申请资源失败就不能直接返回了
    // 所以声明一个rollback_step用来记录要释放那些资源
    // 此外, file_create函数未来是以系统调用的形式来让用户进程使用的, 因此这里直接sys_malloc即可, 得到的内存就是在内核线程的堆中的
    uint8_t rollback_step = 0;
    task_struct_t *cur = running_thread();
    uint32_t user_pgdir_bk = (uint32_t)cur->pgdir;
    cur->pgdir = NULL;
    inode_t *new_file_node = (inode_t *) sys_malloc(sizeof(inode_t));
    if (new_file_node == NULL){
        kprintf("file_create: sys_malloc for inode failed!\n");
        rollback_step = 1;
        goto rollback;              // 因为已经申请了inode_bitmap, 所以跳到后面去释放资源
    }
    inode_init(inode_no, new_file_node);
    cur->pgdir = (uint32_t*)user_pgdir_bk;

    int global_fd_idx = get_free_slot_in_global();
    if (global_fd_idx == -1){
        kprintf("file_create: exceed max open files in system\n");
        rollback_step = 2;
        goto rollback;
    }

    // 填充全局文件描述符
    file_table[global_fd_idx].fd_pos = 0;
    file_table[global_fd_idx].fd_flag = flag;
    file_table[global_fd_idx].fd_inode = new_file_node;
    file_table[global_fd_idx].fd_inode->write_deny = false;

    // 在文件的父目录的目录项表中插入目录项
    dir_entry_t new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(new_dir_entry));
    create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);
    // 而后将目录项写入到磁盘中
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)){
        kprintf("create_file: sync_dir_entry fail!\n");
        rollback_step = 3;
        goto rollback;
    }

    // 因为前面在sync_dir_entry中可能操作了父目录inode, 例如添加了直接块
    // 因此这里还要同步父目录的inode到磁盘
    memset(io_buf, 0, 1024);            // 用完之后内存清0
    inode_sync(current_partition, parent_dir->inode, io_buf);

    // 同步新创建的文件的inode
    memset(io_buf, 0, 1024);
    inode_sync(current_partition, new_file_node, io_buf);

    // 将内存的inode_bitmap写入到磁盘中, block_bitmap已经在sync_dir_entry中写入到磁盘了, 而文件因为创建了之后其中并没有数据
    // 所以并没有给文件分配数据块或者一级间接块, 因此不需要单独位文件同步block_bitmap
    memset(io_buf, 0, 1024);
    bitmap_sync(current_partition, inode_no, INODE_BITMAP);

    // 将新创建的文件的inode插入到open_inode_list中
    list_push(&current_partition->open_inodes, &new_file_node->inode_tag);
    new_file_node->i_open_cnt = 1;

    // 释放磁盘io的缓冲
    sys_free(io_buf);
    // 最后一步, 把全局文件描述符安装到进程的文件描述符表中
    int local_fd = pcb_fd_install(global_fd_idx);
    if (local_fd == -1){
        kprintf("%s: pcb_fd_install failed!\n", __func__);
        return -1;
    }
    return local_fd;

rollback:
    // 释放资源是依次释放的, 申请的顺序是:
    //      1. inode_bitmap
    //      2. 内核线程堆中的inode
    //      3. 系统全局的open_file_table中的一个free slot
    // 所以释放资源的时候, 倒序释放
    switch (rollback_step){
        case 3:
            // sync_dir_entry失败时, 则首先情况系统的文件描述符
            memset(&file_table[global_fd_idx], 0, sizeof(file_desc_t));
            __attribute__ ((fallthrough));
        case 2:
            // get_free_slot失败时, 需要释放内核线程堆中的inode
            cur->pgdir = NULL;
            sys_free(new_file_node);
            cur->pgdir = (uint32_t*) user_pgdir_bk;
            __attribute__ ((fallthrough));
        case 1:
            // sys_malloc(new_file_inode)失败时, 释放申请到的inode bitmap位
            bitmap_set(&current_partition->inode_bitmap, inode_no, 0);
            break;
    }
    // 失败的时候同样要释放io_buf
    sys_free(io_buf);
    return -1;
}


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
int32_t file_open(uint32_t inode_no, uint8_t flag){
    // 获取一个全局文件描述符
    int fd_idx = get_free_slot_in_global();
    if (fd_idx == - 1){
        kprintf("file_open: exceed max global open files\n");
        return -1;
    }

    // 初始化全局文件描述符
    file_table[fd_idx].fd_inode = inode_open(current_partition, inode_no);
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = flag;

    // 如果写文件的话还得判断是否有别的进程也在写文件, 如果没有的话那么就得修改文件的占用位
    bool *write_deny = &file_table[fd_idx].fd_inode->write_deny;
    if (flag & O_WRONLY || flag & O_RDWD){
        intr_status_t old_status = intr_disable();
        if (!(*write_deny)){
            // 当前没有其他进程正在写文件, 则占用文件
            *write_deny = true;
            intr_set_status(old_status);
        } else {
            // 当前有其他进程正在写文件, 则直接失败
            intr_set_status(old_status);
            kprintf("file_open: file is taken by other thread, cannot be written now. Try again later!\n");
            return -1;
        }
    }
    // 如果是读取的话, 或者已经占用了文件的写权限, 则安装即可
    return pcb_fd_install(fd_idx);
}


/**
 * @brief file_close用于关闭file指向的文件
 * 
 * @param file 需要关闭的文件的文件描述符
 * @return int32_t 
 */
int32_t file_close(file_desc_t *file){
    if (file == NULL)
        return -1;
    file->fd_inode->write_deny = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}


/**
 * @brief file_write用于将buf中的count个字节写入到file指向的文件
 * 
 * @param file 需要写入的文件
 * @param buf 需要写入文件的数据
 * @param count 需要写入的字节数
 * @return int32_t 若写入成功则返回写入的字节数; 若失败则返回-1
 */
int32_t file_write(file_desc_t *file, const void *buf, uint32_t count){
    // 目前inode的13个数据块中前12个是直接数据块, 第13个是一个一级数据块, 所以目前最大支持写入的字节数是
    uint32_t max_byts = (12 + 512 / 4) * BLOCK_SIZE;
    if (file->fd_inode->i_size + count > max_byts){
        kprintf(
            "file_write: exceed maximum of file %d bytes, trying to make a file %d bytes",
            max_byts,
            file->fd_inode->i_size + count
        );
        return -1;
    }
    uint8_t *io_buf = sys_malloc(BLOCK_SIZE);
    if (io_buf == NULL){
        kprintf("file_write: sys_malloc for io_buf failed\n");
        return -1;
    }

    uint32_t *all_blocks_lba = (uint32_t*)sys_malloc(BLOCK_SIZE + 48);
    if (all_blocks_lba == NULL) {
        kprintf("file_write: sys_malloc for all_blocks_lba failed\n");
        return -1;
    }

    // 声明变量
    const uint8_t *src = buf;               // 逐字节复制
    uint32_t bytes_written = 0;
    uint32_t size_left = count;
    int32_t block_lba = -1;
    int32_t block_bitmap_idx = 0;

    uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes;
    uint32_t chunk_size;
    int32_t indirect_block_table;
    uint32_t block_idx;

    // 判断文件是否第一次写入, 如果文件是第一次写入, 那么给文件分配一个新的块
    if (file->fd_inode->i_sectors[0] == 0){
        block_lba = block_bitmap_alloc(current_partition);
        if (block_lba == -1){
            kprintf("file_write: block_bitmap_alloc failed!\n");
            return -1;
        }
        file->fd_inode->i_sectors[0] = block_lba;

        // 同步内存中的block_bitmap到磁盘
        block_bitmap_idx = block_lba - current_partition->sb->data_start_lba;
        ASSERT(block_bitmap_idx != 0)       // 不能覆盖根目录
        bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);
    }

    // 写入count个字节前, 先判断一下是写入到上一次没用完的block里还是需要新分配一个block, 然后有一部分写入进去
    // 这里是根据文件的大小来判断现在该写入到哪个数据块, 如果现在要写入的块和之前已经写的块一样的, 那么就不需要分配新的块
    uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
    uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
    ASSERT(file_will_use_blocks <= 140);
    
    uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;

    // 为等下写入数据到磁盘做准备, 把等下要写入的块的lba地址写入到all_blocks_lba中

    // 这里把所有要写入到块的lba地址写入到all_blocks_lba中
    // 1. 不用分配新的数据块, 数据全部写入上一次没写完的块中
    if (add_blocks == 0){
        if (file_will_use_blocks <= 12){            // 写入直接块的话直接写就行了
            // 如果要写入到直接数据块中的话, 并且写入到上一次没写完的块的话, 那么就是idx - 1
            block_idx = file_has_used_blocks - 1;
            all_blocks_lba[block_idx] = file->fd_inode->i_sectors[block_idx];
        } else {                                    // 写入间接块的话还得先读入间接块的地址
            // 因为是上一次已经写过的块, 所以i_sectors[12]一定不等于0
            ASSERT(file->fd_inode->i_sectors[12] != 0)
            indirect_block_table = file->fd_inode->i_sectors[12];
            // 将数据块读入到间接块中
            ide_read(current_partition->my_disk, indirect_block_table, all_blocks_lba + 12, 1);
        }
    // 2. 需要分配一个/多个新的块, 数据即有可能写入新的块, 也有可能写入旧的没用完的块
    } else {        // 需要分配一个/多个新的块来存储新的数据
        // 第一种情况: 新分配的一个/多个块是前12个直接块中的块, 数据将写入前12个块 (数据也必然写入前12个块)
        if (file_will_use_blocks <= 12) {
            // 首先把前一个没用完的块存入到all_blocks_lba中
            block_idx = file_has_used_blocks - 1;
            ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
            all_blocks_lba[block_idx] = file->fd_inode->i_sectors[block_idx];

            // 然后再分配一个/多个新的块
            block_idx = file_has_used_blocks;             // block_idx指向第一个新分配的块
            while (block_idx < file_will_use_blocks){
                block_lba = block_bitmap_alloc(current_partition);
                if (block_lba == -1){
                    kprintf("file_write: block_bitmap_alloc for situation 1 failed\n");
                    return -1;
                }
                // 同步块bitmap
                block_bitmap_idx = block_lba - current_partition->sb->data_start_lba;
                bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);

                // 把新分配的块插入到i_sectors中
                ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                file->fd_inode->i_sectors[block_idx] = all_blocks_lba[block_idx] = block_lba;

                block_idx++;
            }
        // 第二种情况: 新分配的块是一级间接块且数据将同时写入新的块和旧的块, 此时一级间接块必然还没有分配
        } else if (file_has_used_blocks <= 12 && file_will_use_blocks > 12) {
            // 首先把前一个没用完的块存入到all_blocks_lba中
            block_idx = file_has_used_blocks - 1;
            all_blocks_lba[block_idx] = file->fd_inode->i_sectors[block_idx];

            // 创建一级间接块
            block_lba = block_bitmap_alloc(current_partition);
            if (block_lba == -1){
                kprintf("file_write: block_bitmap_alloc for situation 2 faield. Error Code: 1\n");
                return -1;
            }

            // 此时必然没有分配一级间接块
            ASSERT(file->fd_inode->i_sectors[12] == 0);
            indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;

            block_idx = file_has_used_blocks;
            // 由于此时可能即写入旧的直接数据块, 也可能写入新的一级间接块, 所以得循环写入, 即逐块写入
            // 数据块的地址复制到all_blocks_lba中
            while (block_idx < file_will_use_blocks){
                block_lba = block_bitmap_alloc(current_partition);
                if (block_lba == -1){
                    kprintf("file_write: block_bitmap_alloc for situation 2 faield. Error Code: 2\n");
                    return -1;
                }

                if (block_lba < 12){
                    ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                    file->fd_inode->i_sectors[block_idx] = all_blocks_lba[block_idx] = block_lba;
                } else {
                    all_blocks_lba[block_idx] = block_lba;
                }

                // 同步块bitmap
                block_bitmap_idx = block_lba - current_partition->sb->data_start_lba;
                bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);

                block_idx++;
            }
            // 同步一级间接块到硬盘
            ide_write(current_partition->my_disk, indirect_block_table, all_blocks_lba + 12, 1);
        
        // 第三种情况: 数据将只写入一级间接块中, 此时一级间接块已经分配
        } else if (file_has_used_blocks > 12) {
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];

            // 读入一级间接块, 让找到第一个没有使用的间接块
            ide_read(current_partition->my_disk, indirect_block_table, all_blocks_lba + 12, 1);

            block_idx = file_has_used_blocks;
            while (block_idx < file_will_use_blocks){
                block_lba = block_bitmap_alloc(current_partition);
                if (block_lba == -1){
                    kprintf("file_write: block_bitmap_alloc for situation 3 faield.\n");
                    return -1;
                }
                all_blocks_lba[block_idx++] = block_lba;

                // 分配一个块后同步到硬盘
                block_bitmap_idx = block_lba - current_partition->sb->data_start_lba;
                bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);
            }
            // 同步一级间接块到硬盘
            ide_write(current_partition->my_disk, indirect_block_table, all_blocks_lba + 12, 1);
        }
    }



    // 下面开始写数据
    bool first_write_block = true;
    file->fd_pos = file->fd_inode->i_size - 1;              // 操作写文件指针
    // 逐块写入
    while (bytes_written < count){
        memset(io_buf, 0, BLOCK_SIZE);
        sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
        sec_lba = all_blocks_lba[sec_idx];                  // 本次ide_write写入的磁盘块的地址

        // file->fd_inode->i_size表示文件目前的大小,所以整除之后得到的值就是表示上一次写到最后一个块的哪
        // 所以, sec_off_bytes主要给第一次接着写入没有写完的块用的, 后面因为每次都是写入的整个块
        // 所以第二次写入块的时候, sec_off_bytes就是0, 表示本次写入块从哪里开始写
        sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

        // 判断此次要写入的数据的大小, 还是第一次会有区别
        chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;

        // 第一次写入磁盘的时候, 得先把数据从磁盘中读出来, 然后写到后面
        if (first_write_block){
            ide_read(current_partition->my_disk, sec_lba, io_buf, 1);
            first_write_block = false;
        }

        // 然后写入到io_buf中
        memcpy(io_buf + sec_off_bytes, src, chunk_size);
        ide_write(current_partition->my_disk, sec_lba, io_buf, 1);

        kprintf("file_write: data write at 0x%x\n", sec_lba);

        // 更新一下数据
        src += chunk_size;
        file->fd_inode->i_size += chunk_size;
        file->fd_pos += chunk_size;
        bytes_written += chunk_size;
        size_left -= chunk_size;
    }

    // 写完后, 同步一下磁盘中的inode
    void *inode_buf = sys_malloc(BLOCK_SIZE * 2);
    if (inode_buf == NULL){
        kprintf("%s: sys_malloc for inode_buf failed!\n", __func__);
        kprintf("%s: inode failed sync to disk!!\n", __func__);
    } else {
        inode_sync(current_partition, file->fd_inode, inode_buf);
        sys_free(inode_buf);
    }

    // 释放缓冲区
    sys_free(all_blocks_lba);
    sys_free(io_buf);
    return bytes_written;
}


/**
 * @brief file_read用于从file指向的文件从读取count个字节到buf中
 * 
 * @param file 需要读取的文件
 * @param buf 读取的文件将写入的内存缓存
 * @param count 需要读取的字节数
 * @return int32_t 若读取成功, 则返回读取的字节数; 若失败, 则返回-1
 */
int32_t file_read(file_desc_t *file, void *buf, uint32_t count){
    uint8_t *buf_dst = (uint8_t *)buf;
    uint32_t size = count, size_left = size;

    // 如果要读取的字节大于文件剩余的字节, 则读取剩余全部字节
    if (file->fd_pos + count > file->fd_inode->i_size){
        size_left = size = file->fd_inode->i_size - file->fd_pos;
        // 已无内容可读, 则直接返回
        if (size == 0)
            return -1;
    }

    uint8_t *io_buf = (uint8_t *) sys_malloc(BLOCK_SIZE);
    if (io_buf == NULL){
        kprintf("%s: sys_malloc for io_buf failed!\n", __func__);
        return -1;
    }

    // 12 个直接块 + 512 / 4个间接块 = 140个间接块, 140个块 * 4 字节/每块= 512 + 48 字节存储
    uint32_t *all_blocks_lba = (uint32_t *) sys_malloc(BLOCK_SIZE + 48);
    if (all_blocks_lba == NULL){
        kprintf("%s, sys_malloc for all_blocks_lba failed!\n", __func__);
        return -1;
    }

    // 本次读取的block的信息
    uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;              // 本次读取时开始读取的block的idx
    uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;
    uint32_t block_to_read = block_read_end_idx - block_read_start_idx;
    ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);


    // 开始构建all_blocks_lba
    uint32_t block_idx;
    int32_t indirect_block_table;
    // 1. 情况一: 需要读取的块为0, 即读取开始的块和读取结束的块是同一个块
    if (block_to_read == 0){
        ASSERT(block_read_end_idx == block_read_end_idx);
        // 1.1 要读取的文件的块是直接块
        if (block_read_end_idx < 12){
            block_idx = block_read_end_idx;
            all_blocks_lba[block_idx] = file->fd_inode->i_sectors[block_idx];
        // 1.2 要读取的文件块是一级间接块, 则需要把一级间接块读出来
        } else {
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(current_partition->my_disk, indirect_block_table, all_blocks_lba + 12, 1);
        }
    // 2. 情况二: 需要读取多个块, 即读取开始的块和读取结束的块不是同一个块
    } else {
        // 2.1 读取的多个块全是直接块
        if (block_read_end_idx <= 12){
            block_idx = block_read_start_idx;
            // 将多个直接块地址复制到all_blocks_lba中
            while (block_idx <= block_read_end_idx){
                all_blocks_lba[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
            }
        // 2.2 读取的多个块有直接块, 也有间接块
        } else if (block_read_start_idx < 12 && 12 <= block_read_end_idx){
            block_idx = block_read_start_idx;
            // 将多个直接块地址复制到all_blocks_lba中
            while (block_idx < 12){
                all_blocks_lba[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
            }
            // 将间接块地址读取到all_blocks_lba中
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(current_partition->my_disk, indirect_block_table, all_blocks_lba + 12, 1);
        }
    }

    uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
    uint32_t bytes_read = 0;
    while (bytes_read < size){
        sec_idx = file->fd_pos / BLOCK_SIZE;
        sec_lba = all_blocks_lba[sec_idx];
        sec_off_bytes = file->fd_pos % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
        chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;

        memset(io_buf, 0, BLOCK_SIZE);
        ide_read(current_partition->my_disk, sec_lba, io_buf, 1);
        memcpy(buf_dst, io_buf + sec_off_bytes, chunk_size);
        
        buf_dst += chunk_size;
        file->fd_pos += chunk_size;
        bytes_read += chunk_size;
        size_left -= chunk_size;
    }

    sys_free(all_blocks_lba);
    sys_free(io_buf);
    return bytes_read;
}