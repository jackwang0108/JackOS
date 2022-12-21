#include "fs.h"
#include "file.h"
#include "init.h"
#include "syscall-init.h"
#include "syscall.h"
#include "kstdio.h"
#include "print.h"
#include "test.h"
#include "fork.h"
#include "stdio.h"
#include "shell.h"

void init(void);
bool write_user_prog(uint32_t file_size, uint32_t start_lba, char *pathname);
static void write_all_user_prog(void);

extern void sys_clear(void);

int main(void){
    put_str("In kernel now, start happy c time!\n");
    init_all();
    kprintf("main_pid: 0x%x\n", sys_getpid());

    // test_all();

    // write all user program
    write_all_user_prog();

    sys_clear();
    kprintf("[Jack@JackOS /]$ ");

    thread_exit(running_thread(), true);
    return 0;
}


/**
 * @brief init is the first user prog, which starts wish, adopting orphan thread and releasing zombie thread
 */
void init(void){
    uint32_t ret_pid = fork();
    if (ret_pid){
        printf("I am father, my pid is %d, my child pid is %d\n", getpid(), ret_pid);
        int status;
        int32_t child_pid;
        while (1){
            child_pid = wait(&status);
            printf("From init [pid: 1]: adopting a child [pid: %d], status = %d\n", child_pid, status);
        }
    } else {
        printf("I am child, my pid is %d, ret pid is %d\n", getpid(), ret_pid);
        wish();
    }
    printf("init: should not be here");
}

/**
 * @brief write_user_prog finds a file located on lba-th block on sda (JackOS.img) and then writes to sbd (JackOS-fs.img).
 * 
 * @param file_size size of the file
 * @param start_lba start lba of file on sda (JackOS.img), corresponds to makefile, target write_u_prog
 * @param pathname path of the file on sdb
 * @return true write success
 * @return false write fail
 */
bool write_user_prog(uint32_t file_size, uint32_t start_lba, char *pathname){
    uint32_t buf_size = 512;

    char *io_buf = (char *)sys_malloc(buf_size);
    if (io_buf == NULL){
        kprintf("sys_malloc for io_buf failed!\n");
        return false;
    }

    uint32_t lba_to_read = start_lba;
    uint32_t read_times = DIV_CEILING(file_size, buf_size);

    int32_t fd = sys_open(pathname, O_CREAT | O_RDWD);
    if (fd == -1){
        kprintf("sys_open failed!\n");
        return -1;
    } else {
        int32_t g_fd = fd_local2global(fd);
        kprintf("Writing file %s to inode: %d\n", pathname, file_table[g_fd].fd_inode->i_no);
    }

    disk_t *sba = &channels[0].devices[0];
    for (uint32_t i = 0; i < read_times; i++){
        // set to 0, incase of unknow error
        memset((void*) io_buf, -1, buf_size);
        // read data
        ide_read(sba, lba_to_read++, (void*)io_buf, 1);

        // write data
        if (sys_write(fd, (void*)io_buf, buf_size) == -1){
            kprintf("Writing failed! times: %d\n", i);
            return false;
        }
    }

    sys_close(fd);
    sys_free((void*)io_buf);

    return true;
}


/**
 * @brief write all user prog from JackOS.imf to JackOS-fs.img
 * 
 * @details you can get file_size of user program via `ls -l build/xxxx` after you run `make`
 *          start_lba must correspond to `seek` in makefile, target `write_u_prog`
 *          you can decide pathname whatever you like, just make sure parent dir exists on sdb (JackOS-fs.img)
 */
static void write_all_user_prog(void){
    // write a txt file
    char *str = "Hello World!\n";
    int32_t fd = open("/test.txt", O_CREAT | O_RDWD);
    if (fd == -1){
        kprintf("Open /test.txt failed!\n");
    } else {
        if (sys_write(fd, str, strlen(str)) == -1)
            kprintf("Write /test.txt failed!\n");
        else 
            kprintf("Write /test.txt success!\n");
    }


    uint32_t file_sizes[] = {
        14956,          // command/prog_no_arg.c
        15292,          // command/prog_with_arg.c
        15724,          // command/cat.c
        15940,          // command/prog_pipe.c
        16148,          // command/touch.c
        16516           // command/echo.c
    };

    uint32_t start_lbas[] = {
        30000,          // command/prog_no_arg.c
        35000,          // command/prog_with_arg.c
        40000,          // command/cat.c
        45000,          // command/prog_pipe.c
        50000,          // command/touch.c
        55000           // command/echo.c
    };

    char *pathnames[] = {
        "/prog_no_arg",
        "/prog_with_arg",
        "/cat",
        "/prog_pipe",
        "/touch",
        "/echo"
    };

    uint32_t fs = sizeof(file_sizes) / sizeof(uint32_t),
             ss = sizeof(start_lbas) / sizeof(uint32_t),
             ps = sizeof(pathnames) / sizeof(char*);
    
    if (fs != ss && ss != ps)
        PANIC("file_sizes, start_lbas and pathnames mismatch!\n");

    for (uint32_t i = 0; i < fs; i++){
        if (write_user_prog(file_sizes[i], start_lbas[i], pathnames[i]) == -1){
            kprintf("Write user program: %s failed! Given file size: %d, start_lba: %d\n", pathnames[i], file_sizes[i], start_lbas[i]);
            kprintf("User program %s may already exists!\n");
        }
        else
            kprintf("Write user program: %s Success! Given file size: %d, start_lba: %d\n", pathnames[i], file_sizes[i], start_lbas[i]);
    }
}