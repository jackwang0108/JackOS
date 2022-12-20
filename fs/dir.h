#ifndef __FS_DIR_H
#define __FS_DIR_H


#include "stdint.h"
#include "inode.h"
#include "fs.h"
#include "types.h"



extern dir_t root_dir;


/**
 * @brief open_root_dir用于打开分区partition中的根目录
 * 
 * @param partition 
 */
void open_root_dir(partition_t *partition);


/**
 * @brief dir_open用于打开partition指向的分区上inode编号为inode_no的目录
 * 
 * @param partition 指向需要打开的分区的指针
 * @param inode_no 需要打开的目录的在partition指向的分区的inode_table中的index
 */
dir_t *dir_open(partition_t *partition, uint32_t inode_no);


/**
 * @brief dir_close用于关闭dir_open打开的目录
 * 
 * @param dir 指向需要关闭的目录
 */
void dir_close(dir_t *dir);


/**
 * @brief create_dir_entry用于初始化de指向的目录项. 具体来说, 该函数将filename, inode_no以及file_type复制到
 *        de指向的目录项中
 * 
 * @param filename 目录项的文件名
 * @param inode_no 目录项文件的inode编号
 * @param file_type 目录项该文件的标号
 * @param de 需要初始化的目录项
 */
void create_dir_entry(char *filename, uint32_t inode_no, uint8_t file_type, dir_entry_t *de);


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
bool delete_dir_entry(partition_t *partition, dir_t *dir, uint32_t inode_no, void *io_buf);


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
bool search_dir_entry(partition_t *partition, dir_t *dir, const char *name, dir_entry_t *dir_e);



/**
 * @brief sync_dir_entry用于同步de指向的目录项到磁盘中, 即将de指向的目录项写入到其父目录中(parent_dir指向的目录)
 * 
 * @details sync_dir_entry的具体流程就是首先从内存中读出来parent_dir所有的块到内存中, 其中存储着所有的目录项. 而后找出来
 *          第一个非空的目录项, 然后把要写入的目录项复制到内存中, 即将de中的内容复制到parent_dir所有的块在内存中的拷贝
 *          最后将内存中的块写回到磁盘即可
 * 
 * @param parent_dir 指向目录项的父目录
 * @param de 指向需要写入到磁盘中的目录项
 * @param io_buf 因为每次ide_write都是整个磁盘整个磁盘写入的, 因此io_buf是由调用者提供的一个大于512字节的缓冲区. 用于ide_write的时候写入
 * @return true 同步成功
 * @return false 同步失败
 */
bool sync_dir_entry(dir_t *parent_dir, dir_entry_t *de, void *io_buf);



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
dir_entry_t* dir_read(dir_t *dir);


/**
 * @brief dir_is_empty用于判断dir指向的目录是否为空
 * 
 * @param dir 指向需要判断是否为空的目录
 * @return true 目录为空
 * @return false 目录不为空
 */
bool dir_is_empty(dir_t *dir);


/**
 * @brief dir_remove用于删除parent_dir中的child_dir子目录. 注意, dir_remove只能删除空目录
 * 
 * @param parent_dir 需要删除的目录的父目录
 * @param child_dir 需要删除的目录
 * @return int32_t 若删除成功, 则返回0; 若删除失败则返回-1
 */
int32_t dir_remove(dir_t *parent_dir, dir_t *child_dir);

#endif