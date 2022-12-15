#ifndef __FS_FS_H
#define __FS_FS_H

#define MAX_FILE_PER_PARTITION          4096            ///< 每个分区最大的文件数4096个
#define SECTOR_SIZE                     512             ///< 每个扇区的字节数
#define BLOCK_SIZE                      SECTOR_SIZE     ///< 每个扇区的字节数
#define BITS_PER_SECTOR                 4096            ///< 每个扇区的位数
#define MAX_PATH_LEN                    512             ///< 文件路径的最大长度

#include "list.h"
#include "stdint.h"
#include "global.h"



/// @brief 文件类型
typedef enum __file_type_t {
    FT_UNKNOW,
    FT_REGULAR,
    FT_DIRECTORY
} file_type_t;


/// @brief 文件的读写类型
typedef enum __oflags_t {
    O_RDONLY    =   0b000,              ///< 文件只读
    O_WRONLY    =   0b001,              ///< 文件只写
    O_RDWD      =   0b010,              ///< 文件读写
    O_CREAT     =   0b100               ///< 文件若不存在, 则创建文件
} oflags_t;


/// @brief 文件读写位置偏移量
typedef enum __whence_t {
    /// @brief SEEK_SET 会将文件新的读写位置设置为相对文件开头的offset字节处, 此时要求offset为正值
    SEEK_SET  =  1,
    /// @brief SEEK_CUR 会将文件新的读写位置设置为相对当前位置的offset字节处, 此时要求offset正负均可
    SEEK_CUR,
    /// @brief SEEK_END 会将文件新的读写位置设置为相对文件结尾的offset字节处, 此时要求offset为负值
    SEEK_END
} whence_t;


/// @brief search_file函数专用的结构体, 用于递归用
typedef struct __path_search_record {
    char searched_path[MAX_PATH_LEN];
    /**
     * @brief 这里必须声明是__dir_t*, 否则会因为相互引用导致报错,
     *      这里声明成是指针, 这样编译器编译的时候才知道需要预留多少的内存
     */
    struct __dir_t *parent_dir;
    file_type_t file_type;
} path_search_record;


/// @brief 文件属性结构体
typedef struct __stat_t {
    /// @brief 文件的inode编号
    uint32_t st_ino;
    /// @brief 文件的大小
    uint32_t st_size;
    /// @brief 文件类型
    file_type_t st_filetype;
} stat_t;


/**
 * @brief filesys_init用于在磁盘上逐分区搜索文件系统, 如果磁盘上没有文件系统, 则创建一个文件系统
 */
void filesys_init(void);


/**
 * @brief path_parse用于获得文件路径pathname的顶层路径, 顶层路径存入到name_store中
 * 
 * @example pathname="/home/jack", char name_store[10]
 *          path_parse(pathname, name_store) -> return "/jack", *name_store="jack"
 * 
 * @param pathname 需要解析的文件路径
 * @param name_store pathname中的顶层路径
 * @return char* 指向路径名中下一个名字
 */
char *path_parse(char *pathname, char *name_store);


/**
 * @brief search_file用于搜索给定的文件. 若能找到, 则返回要搜索的文件的inode号, 若找不到则返回-1
 * 
 * @param pathname 要搜索的文件的绝对路径
 * @param searched_record 寻找的记录
 * @return int 若成功, 则返回文件的inode号, 若找不到则返回-1
 */
int search_file(const char *pathname, path_search_record *searched_record);


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
bool mount_partition(list_elem_t *elem, int arg);


/**
 * @brief path_depth_cnt用于返回路径pathname的深度. 注意, 所谓路径的深度是指到文件的深度, 例如: /a的深度是1, /a/b的深度
 *        是2, /a/b/c/d/e的深度是5
 * 
 * @param pathname 需要判断深度的路径名
 * @return uint32_t 路径的深度
 */
uint32_t path_depth_cnt(char* pathname);

/**
 * @brief sys_open是open系统调用的实现函数, 用于打开一个指定的文件, 如果文件不存在的话, 则会创建文件, 而后打开该文件
 * 
 * @param pathname 需要打开的文件的绝对路径
 * @param flags 需要打开的文件的读写标志
 * @return int32_t 若成功打开文件, 则返回线程tcb中的文件描述符, 若失败, 则返回-1
 */
int32_t sys_open(const char *pathname, uint8_t flags);



/**
 * @brief sys_close用于关闭文件描述符fd指向的文件.
 * 
 * @param fd 
 * @return int32_t 若关闭成功, 则返回0; 若关闭失败, 则返回-1
 */
int32_t sys_close(int32_t fd);


/**
 * @brief sys_write是write系统调用的实现函数. 用于向fd指定的文件中写入buf中count个字节的数据
 * 
 * @param fd 需要写入的文件描述符
 * @param buf 需要写入的数据所在的缓冲区
 * @param count 需要写入的字节数
 * @return int32_t 成功写入的字节数
 */
int32_t sys_write(int32_t fd, const void *buf, uint32_t count);

/**
 * @brief sys_read是read系统的调用的实现函数. 用于从fd指向的文件中读取count个字节到buf中
 *
 * @param fd 需要读取的文件描述符
 * @param buf 读取的数据将写入的缓冲区
 * @param count 需要读取的字节数
 * @return int32_t 若读取成功, 则返回读取的字节数; 若读取失败, 则返回 -1
 */
int32_t sys_read(int32_t fd, void *buf, uint32_t count);


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
int32_t sys_lseek(int32_t fd, int32_t offset, whence_t whence);


/**
 * @brief sys_unlink是unlink系统调用的实现函数, 用于删除文件(非目录)
 * 
 * @param pathname 需要删除的文件路径名
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t sys_unlink(const char* pathname);


/**
 * @brief sys_mkdir是mkdir系统调用的实现函数. 用于在磁盘上创建路径名位pathname的文件
 * 
 * @param pathname 需要创建的目录的路径名
 * @return int32_t 若创建成功, 则返回0; 若创建失败, 则返回-1
 */
int32_t sys_mkdir(const char* pathname);


/**
 * @brief sys_opendir是opendir系统调用的实现函数. 用于打开目录名为pathname的目录
 * 
 * @param pathname 需要打开目录的路径名
 * @return dir_t* 若打开成功, 则返回目录指针; 若打开失败则返回NULL
 */
struct __dir_t* sys_opendir(const char* pathname);


/**
 * @brief sys_closedir是closedir系统调用的实现函数. 用于关闭一个文件夹
 * 
 * @param dir 需要关闭的文件夹
 * @return int32_t 若关闭成功, 则返回0; 若关闭失败, 则返回-1
 */
int32_t sys_closedir(struct __dir_t *dir);


/**
 * @brief sys_readdir是readdir系统调用的实现函数. 用于读取dir指向的目录中的目录项
 * 
 * @param dir 指向需要读取的目录的指针
 * @return dir_entry_t* 若读取成功, 则返回指向目录项的指针; 若读取失败, 则返回NULL
 */
struct __dir_entry_t *sys_readdir(struct __dir_t *dir);


/**
 * @brief sys_rewinddir是rewinddir系统调用的实现函数. 用于将dir指向的目录中的dir_pose重置为0
 * 
 * @param dir 指向需要重置的文件夹
 */
void sys_rewinddir(struct __dir_t *dir);



/**
 * @brief sys_rmdir是rmdir系统调用的实现函数. 用于删除空目录
 * 
 * @param pathname 需要删除的空目录的路径名
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t sys_rmdir(const char* pathname);


/**
 * @brief sys_getcwd用于将当前运行线程的工作目录的绝对路径写入到buf中
 * 
 * @param buf 由调用者提供存储工作目录路径的缓冲区, 若为NULL则将由操作系统进行分配
 * @param size 若调用者提供buf, 则size为buf的大小
 * @return char* 若成功且buf为NULL, 则操作系统会分配存储工作目录路径的缓冲区, 并返回首地址; 若失败则为NULL
 */
char *sys_getcwd(char *buf, uint32_t size);


/**
 * @brief sys_chdir是chdir系统调用的实现函数. 用于更到当前线程的工作目录为path
 * 
 * @param path 将要修改的工作目录的绝对路径
 * @return int32_t 若修改成功则返回0; 若修改失败则返回-1
 */
int32_t sys_chdir(const char* path);


/**
 * @brief sys_stat用于查询path表示的文件的属性, 并将其填入到buf中
 * 
 * @param path 需要查询属性的文件路径
 * @param buf 由调用者提供的文件属性的缓冲区
 * @return int32_t 若运行成功, 则返回0; 若运行失败, 则返回-1
 */
int32_t sys_stat(const char* path, stat_t *buf);

#endif