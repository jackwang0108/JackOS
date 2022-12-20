#include "fs.h"
#include "file.h"
#include "shell.h"
#include "stdio.h"
#include "assert.h"
#include "string.h"
#include "syscall.h"
#include "builtin_cmd.h"

// shell.c 是用户程序

#define MAX_ARG_NR 16


/// @brief cmd_line用于存储输入的命令
static char cmd_line[MAX_PATH_LEN] = {0};

char final_path[MAX_PATH_LEN] = {0};

/// @brief cwd_cache用于记录当前目录的, 每次cd都会更新
char cwd_cache[64] = {0};

/**
 * @brief print_prompt用于输出终端提示符
 */
static void print_prompt(void){
    printf("[Jack@JackOS %s]$ ", cwd_cache);
}

/**
 * @brief readline用于从键盘缓冲区中读取至多count个字符并存储到buf中.
 *        注意, readline用于从命令行读取用户输入的命令, 因此其中会处理退格键等字符.
 *        并且检测到回车键后返回
 * 
 * @param buf 字符存储的路径
 * @param count 最多读取的字符数
 */
static void readline(char *buf, int32_t count){
    assert(buf != NULL && count > 0);
    char *pos = buf;
    while (read(stdin_no, pos, 1) != -1 && (pos - buf) < count){
        switch (*pos){
            // 命令读取结束
            case '\n':
                __attribute__((fallthrough));
            // 命令读取结束
            case '\r':
                *pos = 0;
                putchar('\n');
                return;
            
            // 删除上一个字符
            case '\b':
                if (buf[0] != '\b'){
                    --pos;
                    putchar('\b');
                }
                break;

            // ctrl + l清屏
            case 'l' - 'a':
                *pos = 0;
                clear();
                print_prompt();
                printf("%s", buf);
                break;
            
            case 'u' - 'a':
                while (buf != pos){
                    putchar('\b');
                    *(pos--) = 0;
                }
                break;
            
            // 默认输出非控制字符
            default:
                putchar(*pos);
                pos++;
        }
    }
    printf("readline: can't find <Enter> in cmd_line, max cached chars are 128.\n");
}


/**
 * @brief cmd_parse用于解析命令行中获取得到的字符串, 而后以token作为分割字符进行解析, 并将解析后的参数存储到argv中.
 *        注意, 该函数不会复制参数到argv中, 而是将指向cmd_str各个参数开始位置的指针复制到argv中
 * 
 * @param cmd_str 将要解析的命令字符串
 * @param agrv 解析后的参数
 * @param token 分割字符
 * @return int32_t 参数个数
 */
static int32_t cmd_split(char *cmd_str, char **agrv, char token){
    assert(cmd_str != NULL);

    // 清空所有的参数列表
    int32_t arg_idx = 0;
    while (arg_idx < MAX_ARG_NR)
        agrv[arg_idx++] = NULL;

    char *next = cmd_str;
    int32_t argc = 0;
    while (*next){
        // 如果是next指向分割字符, 则让next指向下一个字符
        while (*next == token)
            next++;
        
        // 命令字符串最后结尾是空格, 即遇到了 'ls ./ '最后的' '
        if (*next == 0)
            break;
        
        // 存储
        agrv[argc] = next;

        // 让next指向下空格
        while (*next && *next != token)
            next++;

        // 将空格转换为'/0', 方便后续处理, 然后让next指向空格后的下一个字符
        if (*next)
            *next++ = 0;
        
        // 参数超过限制, 返回
        if (argc > MAX_ARG_NR)
            return -1;
        
        argc++;
    }
    return argc;
}


int argc = -1;
char *argv[MAX_ARG_NR];

/**
 * @brief JackOS的Shell
 */
void wish(void){
    cwd_cache[0] = '/';
    while (1){
        print_prompt();
        memset(cmd_line, 0, MAX_PATH_LEN);
        memset(final_path, 0, MAX_PATH_LEN);
        readline(cmd_line, MAX_PATH_LEN);
        if (cmd_line[0] == 0)           // 只输入了一个回车
            continue;
        
        argc = -1;
        argc = cmd_split(cmd_line, argv, ' ');
        if (argc == -1){
            printf("num of arguments exceed %d\n", MAX_ARG_NR);
            continue;
        }

        // run builtin command and external command
        if (!strcmp("ls", argv[0])){                        // ls
            builtin_ls(argc, argv);
        } else if (!strcmp("cd", argv[0])){                 // cd
            if (buildin_cd(argc, argv) != NULL){
                memset(cwd_cache, 0, MAX_PATH_LEN);
                strcpy(cwd_cache, final_path);
            }
        } else if (!strcmp("pwd", argv[0])){                // pwd
            builtin_pwd(argc, argv);
        } else if (!strcmp("ps", argv[0])){                 // ps
            builtin_ps(argc, argv);
        } else if (!strcmp("clear", argv[0])){              // clear
            builtin_clear(argc, argv);
        } else if (!strcmp("mkdir", argv[0])){              // mkdir
            builtin_mkdir(argc, argv);
        } else if (!strcmp("rmdir", argv[0])){              // rmdir
            builtin_rmdir(argc, argv);
        } else if (!strcmp("rm", argv[0])){                 // rm
            builtin_rm(argc, argv);
        } else if (!strcmp("touch", argv[0])){              // touch
            builtin_touch(argc, argv);
        } else {
            // fork to run user program
            int32_t pid = fork();
            // 父进程
            if (pid){
                int32_t status;
                int32_t child_pid = wait(&status);
                if (child_pid == -1)
                    panic("wish: unknow error happened! no child found!\n");
                printf("child_pid: %d, return status: %d\n", child_pid, status);
            // 子进程
            } else {
                make_clear_abs_path(argv[0], final_path);
                argv[0] = final_path;
                // 查找文件, 判断文件是否在存在
                stat_t file_stat;
                memset(&file_stat, 0, sizeof(stat_t));
                if (stat(argv[0], &file_stat) == -1){
                    printf("wish: cannot access %s: No such file or directory\n", argv[0]);
                    exit(-1);
                }
                execv(argv[0], (char**) argv);
            }
        }

        // clear argument
        int32_t arg_idx = 0;
        while (arg_idx < MAX_ARG_NR)
            argv[arg_idx++] = NULL;
    }
    printf("should not be here!\n");
}