#include "dir.h"
#include "ide.h"
#include "file.h"
#include "debug.h"
#include "inode.h"
#include "string.h"
#include "kstdio.h"

dir_t root_dir;


/**
 * @brief open_root_dir用于打开分区partition中的根目录
 * 
 * @param partition 
 */
void open_root_dir(partition_t *partition){
    root_dir.inode = inode_open(partition, partition->sb->root_inode_no);
    root_dir.dir_pos = 0;
}


/**
 * @brief dir_open用于打开partition指向的分区上inode编号为inode_no的目录
 * 
 * @param partition 指向需要打开的分区的指针
 * @param inode_no 需要打开的目录的在partition指向的分区的inode_table中的index
 */
dir_t *dir_open(partition_t *partition, uint32_t inode_no){
    dir_t *pdir = (dir_t *) sys_malloc(sizeof(dir_t));
    pdir->inode = inode_open(partition, inode_no);
    pdir->dir_pos = 0;
    return pdir;
}


/**
 * @brief dir_close用于关闭dir_open打开的目录
 * 
 * @param dir 指向需要关闭的目录
 */
void dir_close(dir_t *dir){
    // 根目录不能被关闭, 因为root_dir是在内核中, 不是在内核的线程中
    if (dir == &root_dir)   
        return;
    // 释放dir_open中sys_malloc得到的堆
    inode_close(dir->inode);
    sys_free(dir);
}


/**
 * @brief create_dir_entry用于初始化de指向的目录项. 具体来说, 该函数将filename, inode_no以及file_type复制到
 *        de指向的目录项中
 * 
 * @param filename 目录项的文件名
 * @param inode_no 目录项文件的inode编号
 * @param file_type 目录项该文件的标号
 * @param de 需要初始化的目录项
 */
void create_dir_entry(char *filename, uint32_t inode_no, uint8_t file_type, dir_entry_t *de){
    ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);
    // 初始化目录项
    memcpy(de->filename, filename, strlen(filename));
    de->i_no = inode_no;
    de->f_type = file_type;
}


extern partition_t *current_partition;

/**
 * @brief delete_dir_entry用于删除partition分区中dir目录中的inode编号位inode_no的文件
 * 
 * @param partition 需要删除的文件/文件夹所在的分区
 * @param dir 需要删除的文件/文件夹所在的目录
 * @param inode_no 需要删除的文件/文件夹的inode编号
 * @param io_buf 磁盘io时的缓冲区, 由调用者提供
 * @return true 删除成功
 * @return false 删除失败, 因为没有找到需要删除的目录项
 */
bool delete_dir_entry(partition_t *partition, dir_t *dir, uint32_t inode_no, void *io_buf){
    inode_t *dir_inode = dir->inode;

    // 收集目录所有块的地址
    // 收集直接块
    uint32_t block_idx = 0, all_block_lba[140] = {0};
    while (block_idx < 12){
        all_block_lba[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    // 收集一级块
    if (dir_inode->i_sectors[12] != 0){
        ide_read(partition->my_disk, dir_inode->i_sectors[12], all_block_lba + 12, 1);
    }

    // 目录项是不会跨扇区的
    uint32_t dir_entry_size = partition->sb->dir_entry_size;
    uint32_t dir_entry_per_sec = (SECTOR_SIZE / dir_entry_size);

    // 遍历目录文件的所有块来循找目录项
    dir_entry_t *de = (dir_entry_t*) io_buf;
    dir_entry_t *de_found = NULL;
    uint8_t dir_entry_idx, dir_entry_cnt;
    bool is_dir_first_block = false;

    block_idx = 0;
    while (block_idx < 140){
        is_dir_first_block = false;
        // 这个块没有分配
        if (all_block_lba[block_idx] == 0){
            block_idx++;
            continue;
        }

        // 读取块中的所有内容 (512 / 4 = 103个目录项)
        dir_entry_idx = dir_entry_cnt = 0;
        memset(io_buf, 0, SECTOR_SIZE);
        ide_read(partition->my_disk, all_block_lba[block_idx], io_buf, 1);

        // 遍历所有目录项
        while (dir_entry_idx < dir_entry_per_sec && de_found == NULL){
            // 确保文件有效
            if ((de + dir_entry_idx)->f_type != FT_UNKNOW){
                // 情况1: 当前正在遍历第一个扇区, 则稍后不能删除这个扇区
                if (!strcmp( (de + dir_entry_idx)->filename, "."))
                    is_dir_first_block = true;
                // 情况2: 不是 . 和 .. 这两个目录
                else if (
                    strcmp((de + dir_entry_idx)->filename, ".")
                        &&
                    strcmp((de + dir_entry_idx)->filename, "..")
                ){  
                    // 记录此扇区的dir_entry数, 等下用于判断是否该删除该扇区
                    dir_entry_cnt++;
                    // 若找到了要删除的目录项, 则记录下来
                    // 文件名不是文件的唯一标识, 因为会有同名的目录/文件
                    if ((de + dir_entry_idx)->i_no == inode_no){
                        ASSERT(de_found == NULL);
                        de_found = de + dir_entry_idx;
                    }
                }
            }
            // 没有找到
            dir_entry_idx++;
        }

        // 若当前扇区没有找到, 则读取下一个扇区并进行查找
        if (de_found == NULL){
            block_idx++;
            continue;
        }

        // 运行到这里必然是已经找到了目录项了
        ASSERT(dir_entry_cnt >= 1)              // 必然有 '.' 和 '..'
        // 当前扇区只有要删除的目录项这一个目录项, 并且不是第一个扇区的话, 那就就直接删除这个块
        // 由于该块可能是直接块也有可能是一级间接块, 所以得分情况
        if (dir_entry_cnt == 1 && !is_dir_first_block){
            // 1. 位图中回收该块
            uint32_t block_bitmap_idx = all_block_lba[block_idx] - partition->sb->block_bitmap_lba;
            bitmap_set(&partition->block_bitmap, block_bitmap_idx, 0);
            bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);
            
            // 2. 去处块地址
            // 2.1 直接块的话直接去掉
            if (block_idx < 12){
                dir_inode->i_sectors[block_idx] = 0;
            // 2.2 如果要去掉的块是一级间接块中的块, 还得看一级间接块中还有没有块了, 如果没有了那么一级间接块也要回收
            } else {
                uint32_t indirect_blocks = 0;
                uint32_t indirect_block_idx = 12;
                while (indirect_block_idx < 140){
                    if (all_block_lba[indirect_block_idx] != 0)
                        indirect_block_idx++;
                }
                // 至少有当前数据块
                ASSERT(indirect_blocks >= 1);

                // 如果一级间接块中还有别的块, 则将其删除后写回到磁盘中
                if (indirect_blocks > 1){
                    all_block_lba[block_idx] = 0;
                    ide_write(partition->my_disk, dir_inode->i_sectors[12], all_block_lba + 12, 1);
                // 如果一级间接块中没有别的块了, 则连同一级间接块一起删除
                } else {
                    block_bitmap_idx = dir_inode->i_sectors[12] - partition->sb->data_start_lba;
                    bitmap_set(&partition->block_bitmap, block_bitmap_idx, 0);
                    bitmap_sync(partition, block_bitmap_idx, BLOCK_BITMAP);
                    dir_inode->i_sectors[12] = 0;
                }
            }
        // 如果是第一个块, 或者该块中还有别目录项导致不需要删除整个块
        } else {
            memset(de_found, 0, dir_entry_size);
            ide_write(partition->my_disk, all_block_lba[block_idx], io_buf, 1);
        }

        // 更新inode中的信息, 并且同步到磁盘
        ASSERT(dir_inode->i_size >= dir_entry_size);
        dir_inode->i_size -= dir_entry_size;
        memset(io_buf, 0, SECTOR_SIZE * 2);
        inode_sync(partition, dir_inode, io_buf);

        return true;
    }

    // 所有的块中都没有找到需要删除的inode
    return false;
}


/**
 * @brief search_dir_entry用于在partition指向的分区中dir指向的目录中寻找名称为name的文件或者目录, 找到后将其目录项存入dir_e中
 * 
 * @details 一个目录文件用一个inode存储, 一个inode中有12个直接块, 即直接存储文件数据的块, 1个间接项, 即存储存储文件数据的块的LBA号,
 *          因此存储一个文件, 最多用 12 + 512 / 4 (一个扇区512字节, 一个目录项4字节) = 140个块
 *          因此, 存储目录文件最多需要140个块. 而这140个块中, 每个块内包含的都是目录项, 所以逐块检查目录项.
 *          即逐个读取块, 然后检查其中的目录项
 * 
 * @param partition 指向要寻找的文件或者目录在的扇区
 * @param dir 指向要寻找的文件或者目录在的父目录
 * @param name 要寻找的文件或者目录的名称
 * @param dir_e 存储寻找到的问津或者目录的目录项
 * @return true 若在dir指向的文件之找到了要寻找的目录或者文件, 则返回true
 * @return false 若在dir指向的文件之没有找到要寻找的目录或者文件, 则返回false
 */
bool search_dir_entry(partition_t *partition, dir_t *dir, const char *name, dir_entry_t *dir_e){
    // 分配存储140个块需要的内存
    uint32_t *all_blocks_lba = (uint32_t *) sys_malloc(48 + 512);
    memset(all_blocks_lba, 0, 48 + 512);
    if (all_blocks_lba == NULL){
        kprintf("search_dir_entry: sys_malloc fail!");;
        return false;
    }

    // 复制目录项中的数据
    uint32_t block_idx = 0;
    // 目录文件的inode的前12个块是直接块
    while (block_idx < 12){
        all_blocks_lba[block_idx] = dir->inode->i_sectors[block_idx];
        block_idx++;
    }
    // 目录文件有一级间接块, 则读入间接块数据
    if (dir->inode->i_sectors[12] != 0)
        ide_read(partition->my_disk, dir->inode->i_sectors[12], all_blocks_lba + 12, 1);


    // 逐个检查目录项
    uint8_t* buf = (uint8_t *) sys_malloc(SECTOR_SIZE);
    dir_entry_t *de = (dir_entry_t *)buf;

    uint32_t dir_entry_size = partition->sb->dir_entry_size;
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;

    block_idx = 0;
    /**
     * @brief 一个目录文件用一个inode存储, 一个inode中有12个直接项, 1个间接项,
     *      因此存储目录文件, 最多用 12 + 512 / 4 (一个扇区512字节, 一个目录项4字节) = 140个块
     *      而目录文件的这140个块中, 每个块内包含的都是目录项, 所以逐块检查目录项
     */
    uint32_t block_cnt = 140;
    while (block_idx < block_cnt) {
        // 块地址为0, 则表示前面在读取块地址的时候没有读取到
        if (all_blocks_lba[block_idx] == 0){
            block_idx++;
            continue;
        }
        // 读取块, 获得目录的目录项
        uint32_t lba = all_blocks_lba[block_idx];
        ide_read(partition->my_disk, lba, buf, 1);

        uint32_t dir_entry_idx = 0;
        while (dir_entry_idx < dir_entry_cnt){
            // 若找到了, 则复制目录项到dir_e中
            if (!strcmp(de->filename, name)){
                memcpy(dir_e, de, dir_entry_size);
                sys_free(buf);
                sys_free(all_blocks_lba);
                return true;
            }
            de++;
            dir_entry_idx++;
        }

        // 已经检查完该块了, 检查下一个块
        block_idx++;
        de = (dir_entry_t*) buf;            // 让de重新指向buf的开头
        memset(buf, 0, SECTOR_SIZE);        // buf清0, 为下次读取块做准备
    }
    
    sys_free(buf);
    sys_free(all_blocks_lba);
    return false;
}


// 在fs.c中声明
extern partition_t *current_partition;

/**
 * @brief sync_dir_entry用于同步de指向的目录项到磁盘中, 即将de指向的目录项写入到其父目录中(parent_dir指向的目录)
 * 
 * @details sync_dir_entry的具体流程就是首先从磁盘中读出来parent_dir所有的块到内存中, 其中存储着所有的目录项. 而后找出来
 *          第一个非空的目录项, 然后把要写入的目录项复制到内存中, 即将de中的内容复制到parent_dir所有的块在内存中的拷贝
 *          最后将内存中的块写回到磁盘即可
 * 
 * @param parent_dir 指向目录项的父目录
 * @param de 指向需要写入到磁盘中的目录项
 * @param io_buf 因为每次ide_write都是整个磁盘整个磁盘写入的, 因此io_buf是由调用者提供的一个大于512字节的缓冲区. 用于ide_write的时候写入
 * @return true 同步成功
 * @return false 同步失败
 */
bool sync_dir_entry(dir_t *parent_dir, dir_entry_t *de, void *io_buf){

    // 数据声明, 给后面准备的
    inode_t *dir_inode = parent_dir->inode;
    uint32_t dir_size = dir_inode->i_size;
    uint32_t dir_entry_size = current_partition->sb->dir_entry_size;
    // 因为是dir, 所以size必须能够被整除
    ASSERT(dir_size % dir_entry_size == 0);

    uint32_t dir_entry_per_sec = (SECTOR_SIZE / dir_entry_size);

    int32_t block_lba = -1;
    uint8_t block_idx = 0;
    uint32_t all_blocks_lba[140] = {0};                 // 140个块的lba编号

    // 注意, 这里不需要读出全部的140个块, 读出前12个直接块就可以直接开始搜索了
    // 前12个块是直接块, 所以直接复制lba地址即可
    while (block_idx < 12){
        all_blocks_lba[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }


    // 注意:
    //      1. 一个文件(包括目录文件)最多占用140个块, 一个块512字节, 所以一个文件最大71680字节
    //         但是并不是所有文件(包括目录)文件会用完这140个块, 所以优先从前面的12个块开始搜索
    //         即直接开始搜索, 而非读取完目录项的140个块之后再开始检查.
    //      2. 并且, 有可能存在一个情况, 就是目录文件本身可能一开始只用了第一个直接块. 那么在这个
    //         时候, 后面的11个直接块, 包括一级间接块里面其实都是没有记录块的地址的. 此时, 如果
    //         我们要向一个没有分配的块中写入的目录项的话, 还需要先分配一个块, 然后把该块的LBA地址
    //         写入到inode.i_sectors[0-11]或者inode.i_sectors[12]一级间接块中
    //      3. 最后, 因为有可能会删除文件, 所以目录文件中的目录项表并不是连续的, 所以得一个个检查

    // 综上, 下面这部分循找空余位置的代码, 逻辑就是从第一个直接块开始顺序检查
    // 如果:
    //      1. 某个直接块的lba为0, 则表示前面的直接块都已经装满了目录项了, 则此时需要重新分配一个快
    //         而后将需要写入的目录项写入到新增的块中
    //          1.1 若此时新分配的块是前11个块, 则分配得到块就是数据块, 则此时直接将目录项写入到数据块中
    //          1.2 若前面的12个直接块满了, 则此时新分配的块是第13个块是一级间接块, 此时还需要再分配一个数据块
    //              而后将新的数据块的lba地址写入到一级间接块中, 最后再将目录项写入到数据块中
    //          1.3 若此时13个数据块都已经填充满, 则此时分配的是写入到一级间接块中的数据块, 此时将目录项写入到
    //              数据块中, 然后将数据块地址写入到第13个间接块中
    block_idx = 0;
    int32_t block_bitmap_idx = -1;
    dir_entry_t *de_temp = (dir_entry_t *) io_buf;           // 等会用于遍历目录
    while (block_idx < 140){
        block_bitmap_idx = -1;
        // 情况一, 块没分配, 此时分配一个新的块, 然后将要插入的目录项写入其中
        if (all_blocks_lba[block_idx] == 0){
            block_lba = block_bitmap_alloc(current_partition);
            if (block_lba == -1){
                kprintf("alloc block bitmap for sync_dir_entry failed!\n");
                return false;
            }

            // 分配一个块后, 同步在磁盘中block bitmap
            block_bitmap_idx = block_lba - current_partition->sb->data_start_lba;
            ASSERT(block_bitmap_idx != -1);
            bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);

            // 而后将新得到的块的lba地址记录到直接块或者一级间接块中
            block_bitmap_idx = -1;
            if (block_idx < 12){
                // 地址记录到直接块中, 同时也复制到all_block_lba中
                dir_inode->i_sectors[block_idx] = all_blocks_lba[block_idx] = block_lba;
            } else if (block_idx == 12) {
                // 如果是一级间接块未分配, 则先给一级间接块分配, 而后再给一级间接块中的第一个块分配
                dir_inode->i_sectors[12] = block_lba;
                // 然后再给一级间接块中的第一个lba地址分配一个新的块
                block_lba = -1;
                block_lba = block_bitmap_alloc(current_partition);
                if (block_lba == -1){
                    // 如果分配一个新的块失败, 则归还一级间接块
                    block_bitmap_idx = dir_inode->i_sectors[12] - current_partition->sb->data_start_lba;
                    bitmap_set(&current_partition->block_bitmap, block_bitmap_idx, 0);
                    dir_inode->i_sectors[12] = 0;
                    kprintf("sync_dir_entry: alloc block bitmap for failed\n");
                    return false;
                }

                // 分配成功, 则需要重新同步一次
                block_bitmap_idx = block_lba - current_partition->sb->data_start_lba;
                ASSERT(block_bitmap_idx != -1);
                bitmap_sync(current_partition, block_bitmap_idx, BLOCK_BITMAP);

                // 稍后要搜索的块不能是一级间接块, 应该是一级间接块中指向的第一个块
                all_blocks_lba[12] = block_lba;
                // 把新分配得到的一级间接块中的内容, 即all_blocks_lba[12:], 写入到磁盘中中
                ide_write(current_partition->my_disk, dir_inode->i_sectors[12], all_blocks_lba + 12, 1);
            } else {
                // 一级间接块中的数据块未分配, 则插入到一级间接块后在写入到磁盘
                all_blocks_lba[block_idx] = block_lba;
                ide_write(current_partition->my_disk, dir_inode->i_sectors[12], all_blocks_lba + 12, 1);
            }

            // 不论如何, 到这里lba现在指向的, 都是数据块了, 此时将新目录项de写入到数据块中
            memset(io_buf, 0, 512);
            memcpy(io_buf, de, dir_entry_size);
            ide_write(current_partition->my_disk, all_blocks_lba[block_idx], io_buf, 1);
            dir_inode->i_size += dir_entry_size;
            return true;
        }


        // 情况二, 块已经分配, 此时读入新的块, 然后在块中查找空的目录项即可
        uint8_t dir_entry_idx = 0;
        ide_read(current_partition->my_disk, all_blocks_lba[block_idx], io_buf, 1);
        while (dir_entry_idx < dir_entry_per_sec){
            // 根据File Type的值来判断文件类型, 未初始化或者被删除的文件, 文件类型都是FT_UNKONWN
            if ((de_temp + dir_entry_idx)->f_type == FT_UNKNOW){
                // 把de中的内容写入到
                memcpy(de_temp + dir_entry_idx, de, dir_entry_size);
                ide_write(current_partition->my_disk, all_blocks_lba[block_idx], io_buf, 1);
                dir_inode->i_size += dir_entry_size;
                return true;
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    // 140个块都遍历完了, 输出信息结束
    kprintf("dir if full!\n");
    return false;
}


/**
 * @brief dir_read用于读取目录, 每次读取都将读取下一个目录项.
 * 
 * @details 注意, dir_read的读取原理就是利用dir_t.dir_pos来记录当前目录的读取位置
 *          每次调用dir_read的时候, 都会更新dir_t.dir_pos, 使其指向下一个目录项
 *          知道之后dir_t.dir_pos的值大于dir_t.dir_size, 即已经读取完了所有的目录项
 * 
 * @param dir 需要读取的目录
 * @return dir_entry_t* 若读取成功, 则返回下一个目录项; 若失败, 则返回NULL
 */
dir_entry_t* dir_read(dir_t *dir){
    dir_entry_t *dir_e = (dir_entry_t*) dir->dir_buf;
    inode_t *dir_inode = dir->inode;

    // 复制140个块的lba地址
    // uint32_t block_cnt = 12;
    uint32_t block_idx = 0, dir_entry_idx = 0;
    uint32_t all_blocks_lba[140] = {0};
    // 复制直接块
    while (block_idx < 12){
        all_blocks_lba[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    // 复制一级间接块
    if (dir_inode->i_sectors[12] != 0){
        ide_read(current_partition->my_disk, dir_inode->i_sectors[12], all_blocks_lba + 12, 1);
        // block_cnt = 140;
    }


    // 逐块遍历遍历目录项
    block_idx = 0;
    uint32_t cur_dir_entry_pos = 0;
    uint32_t dir_entry_size = current_partition->sb->dir_entry_size;
    uint32_t dir_entry_per_sce = SECTOR_SIZE / dir_entry_size;
    while (dir->dir_pos < dir_inode->i_size){
        // 已经遍历完了所有的dir_entry, 此时直接返回NULL
        if (dir->dir_pos >= dir_inode->i_size)
            return NULL;
        
        // 如果当前块lba的地址为0, 则继续下一个块
        if (all_blocks_lba[block_idx] == 0){
            block_idx++;
            continue;
        }

        memset(dir_e, 0, SECTOR_SIZE);
        ide_read(current_partition->my_disk, all_blocks_lba[block_idx], dir_e, 1);
        dir_entry_idx = 0;

        // 遍历扇区内所有目录项
        while (dir_entry_idx < dir_entry_per_sce){
            // 如果当前文件的类型不是FT_UNKNOWN, 即不是0
            if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOW){
                if (cur_dir_entry_pos < dir->dir_pos){
                    cur_dir_entry_pos += dir_entry_size;
                    dir_entry_idx++;
                    continue;
                }
                ASSERT(cur_dir_entry_pos == dir->dir_pos);
                // 更新dir_pos
                dir->dir_pos += dir_entry_size;
                return dir_e + dir_entry_idx;
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    return NULL;
}


/**
 * @brief dir_is_empty用于判断dir指向的目录是否为空
 * 
 * @param dir 指向需要判断是否为空的目录
 * @return true 目录为空
 * @return false 目录不为空
 */
bool dir_is_empty(dir_t *dir){
    inode_t *dir_inode = dir->inode;
    // 如果目录中只有'.'和'..', 那么目录就是空
    return (dir_inode->i_size == current_partition->sb->dir_entry_size * 2);
}


/**
 * @brief dir_remove用于删除parent_dir中的child_dir子目录. 注意, dir_remove只能删除空目录
 * 
 * @param parent_dir 需要删除的目录的父目录
 * @param child_dir 需要删除的目录
 * @return int32_t 若删除成功, 则返回0; 若删除失败则返回-1
 */
int32_t dir_remove(dir_t *parent_dir, dir_t *child_dir){
    inode_t *child_dir_inode = child_dir->inode;

    // 确保是空目录, 此时只有i_sectors[0]表示的块中有'.'和'..'
    int32_t block_idx = 1;
    while(block_idx < 12){
        ASSERT(child_dir_inode->i_sectors[block_idx] == 0);
        block_idx++;
    }

    void *io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (io_buf == NULL){
        kprintf("%s: malloc for io_buf failed\n", __func__);
        return -1;
    }

    // 在父目录中删除子目录child_dir对应的目录项
    delete_dir_entry(current_partition, parent_dir, child_dir_inode->i_no, io_buf);

    // 回收inode中i_sector所占用的扇区: 1. 修改inode_bitmap  2. 修改block_bitmap
    inode_release(current_partition, child_dir_inode->i_no);
    
    sys_free(io_buf);

    return 0;
}