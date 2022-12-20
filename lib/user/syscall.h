#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "types.h"
#include "stdint.h"

typedef enum __SYSCALL_NR_t {
    SYS_GETPID,
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_READ,
    SYS_LSEEK,
    SYS_UNLINK,
    SYS_MKDIR,
    SYS_OPENDIR,
    SYS_CLOSEDIR,
    SYS_READDIR,
    SYS_REWINDDIR,
    SYS_RMDIR,
    SYS_GETCWD,
    SYS_CHDIR,
    SYS_STAT,
    SYS_FORK,
    SYS_PUTCHAR,
    SYS_CLEAR,
    SYS_PS,
    SYS_EXECV,
    SYS_WAIT,
    SYS_EXIT
} SYSCALL_NR_t;


/**
 * @brief getpid返回当前用户进程的PID
 * @return uint32_t 用户进程的PID
 */
uint32_t getpid(void);


/**
 * @brief write用于向fd指定的文件中写入buf中count个字节的数据
 * 
 * @param fd 需要写入的文件描述符
 * @param buf 需要写入的数据所在的缓冲区
 * @param count 需要写入的字节数
 * @return int32_t 成功写入的字节数
 */
int32_t write(int32_t fd, const void *buf, uint32_t count);


/**
 * @brief malloc系统调用将从当前进程的堆中申请size个字节的内存
 * @param size 要申请字节数
 * @return void* 若申请成功, 则返回申请得到的内存第一个字节的地址; 失败则返回NULL
 */
void* malloc(uint32_t size);

/**
 * @brief free系统调用用于将ptr执向的内存归还到当前进程的堆中. 注意, 归还的
 *      内存必须是malloc系统调用分配得到的内存
 * @param ptr 指向要归还的内存的指针
 */
void free(void *ptr);


/**
 * @brief open系统调用用于打开一个指定的文件, 如果文件不存在的话, 则会创建文件, 而后打开该文件
 * 
 * @param pathname 需要打开的文件的绝对路径
 * @param flags 需要打开的文件的读写标志
 * @return int32_t 若成功打开文件, 则返回线程tcb中的文件描述符, 若失败, 则返回-1
 */
int32_t open(const char *pathname, uint8_t flags);


/**
 * @brief close系统调用用于关闭文件描述符fd指向的文件.
 * 
 * @param fd 需要关闭的文件的文件描述符
 * @return int32_t 若关闭成功, 则返回0; 若关闭失败, 则返回-1
 */
int32_t close(int32_t fd);


/**
 * @brief read系统的调用用于从fd指向的文件中读取count个字节到buf中
 *
 * @param fd 需要读取的文件描述符
 * @param buf 读取的数据将写入的缓冲区
 * @param count 需要读取的字节数
 * @return int32_t 若读取成功, 则返回读取的字节数; 若读取失败, 则返回 -1
 */
int32_t read(int32_t fd, void *buf, uint32_t count);


/**
 * @brief lseek系统调用用于设置fd指向的文件的读写指针
 * 
 * @param fd 需要设置读写指针的文件
 * @param off 要设置的偏移量
 * @param whence 设置新的偏移量的方式. 其中:
 *          1. SEEK_SET 会将文件新的读写位置设置为相对文件开头的offset字节处, 此时要求offset为正值
 *          2. SEEK_CUR 会将文件新的读写位置设置为相对当前位置的offset字节处, 此时要求offset正负均可
 *          3. SEEK_END 会将文件新的读写位置设置为相对文件结尾的offset字节处, 此时要求offset为负值
 * @return int32_t 若设置成功, 则返回新的偏移量; 若设置失败, 则返回-1
 */
int32_t lseek(int32_t fd, int32_t off, whence_t whence);


/**
 * @brief unlink系统调用的实现函数, 用于删除文件(非目录)
 * 
 * @param pathname 需要删除的文件路径名
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t unlink(const char* pathname);


/**
 * @brief mkdir系统调用用于在磁盘上创建路径名位pathname的文件
 * 
 * @param pathname 需要创建的目录的路径名
 * @return int32_t 若创建成功, 则返回0; 若创建失败, 则返回-1
 */
int32_t mkdir(const char* pathname);


/**
 * @brief opendir系统调用用于打开目录名为pathname的目录
 * 
 * @param pathname 需要打开目录的路径名
 * @return dir_t* 若打开成功, 则返回目录指针; 若打开失败则返回NULL
 */
dir_t* opendir(const char* pathname);


/**
 * @brief closedir系统调用用于关闭一个文件夹
 * 
 * @param dir 需要关闭的文件夹
 * @return int32_t 若关闭成功, 则返回0; 若关闭失败, 则返回-1
 */
int32_t closedir(dir_t *dir);


/**
 * @brief readdir系统调用的实现函数. 用于读取dir指向的目录中的目录项
 * 
 * @param dir 指向需要读取的目录的指针
 * @return dir_entry_t* 若读取成功, 则返回指向目录项的指针; 若读取失败, 则返回NULL
 */
dir_entry_t *readdir(dir_t *dir);


/**
 * @brief rewinddir系统调用用于将dir指向的目录中的dir_pose重置为0
 * 
 * @param dir 指向需要重置的文件夹
 */
void rewinddir(dir_t *dir);


/**
 * @brief rmdir系统调用用于删除空目录
 * 
 * @param pathname 需要删除的空目录的路径名
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t rmdir(const char* pathname);


/**
 * @brief getcwd系统调用用于将当前运行线程的工作目录的绝对路径写入到buf中
 * 
 * @param buf 由调用者提供存储工作目录路径的缓冲区, 若为NULL则将由操作系统进行分配
 * @param size 若调用者提供buf, 则size为buf的大小
 * @return char* 若成功且buf为NULL, 则操作系统会分配存储工作目录路径的缓冲区, 并返回首地址; 若失败则为NULL
 */
char *getcwd(char *buf, uint32_t size);


/**
 * @brief chdir系统调用用于更到当前线程的工作目录为path
 * 
 * @param path 将要修改的工作目录的绝对路径
 * @return int32_t 若修改成功则返回0; 若修改失败则返回-1
 */
int32_t chdir(const char* path);


/**
 * @brief stat用于查询path表示的文件的属性, 并将其填入到buf中
 * 
 * @param path 需要查询属性的文件路径
 * @param buf 由调用者提供的文件属性的缓冲区
 * @return int32_t 若运行成功, 则返回0; 若运行失败, 则返回-1
 */
int32_t stat(const char* path, stat_t *buf);


/**
 * @brief fork系统调用于复制出来一个子进程. 子进程的数据和代码和父进程的数据代码一模一样
 * 
 * @return pid_t 父进程返回子进程的pid; 子进程返回0
 */
pid_t fork(void);

/**
 * @brief putchar系统调用用于输出一个字符
 * 
 * @param char_ascii 需要输出的字符的Ascii码
 */
void putchar(char char_ascii);


/**
 * @brief clear系统调用用于清空当前屏幕
 */
void clear(void);


/**
 * @brief ps系统调用用于输出all_thread_list中所有的节点
 */
void ps(void);


/**
 * @brief execv系统调用用于将path指向的程序加载到内存中, 而后用该程序替换当前程序
 * 
 * @param path 程序的绝对路径
 * @param argv 参数列表
 * @return int32_t 若运行成功, 则返回0 (其实不会返回); 若运行失败, 则返回-1
 */
int32_t execv(const char* path, char *argv[]);


/**
 * @brief wait系统调用用于让父进程(调用wait的进程)等待子进程调用exit退出, 并将子进程的返回值保存到status中
 * 
 * @param status 子进程的退出状态
 * @return pid_t 若等待成功, 则返回子进程的pid; 若等待失败, 则返回-1
 */
pid_t wait(int32_t *status);


/**
 * @brief exit系统调用用于主动结束调用的进程
 */
void exit(int32_t status);

#endif