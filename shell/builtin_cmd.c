#include "fs.h"
#include "file.h"
#include "stdio.h"
#include "shell.h"
#include "assert.h"
#include "string.h"
#include "syscall.h"
#include "builtin_cmd.h"

/**
 * @brief wash_path用于将包含相对路径的old_path转换为绝对路径后存入new_abs_path.
 *        例如将/a/b/../c转换为/a/c
 * 
 * @param old_abs_path 包含相对路径的old_path
 * @param new_abs_path 新的绝对路径
 */
static void wash_path(char *old_abs_path, char *new_abs_path){
    assert(old_abs_path[0] == '/');
    
    char name[MAX_FILE_NAME_LEN] = {0};
    char *sub_path = old_abs_path;
    sub_path = path_parse(sub_path, name);;
    // 只输入了 '/'
    if (name[0] == 0){
        new_abs_path[0] = '/';
        new_abs_path[1] = 0;
        return;
    }

    // 将new_abs_path "清空"
    new_abs_path[0] = 0;
    // 拼接根目录
    strcat(new_abs_path, "/");

    // 逐层向下遍历目录
    while (name[0]){
        // 如果当前目录是上级目录, 则寻找上一个'/', 然后删除掉上一个'/'之后的内容
        // 即将'/jack/desktop/..'设置为'/jack0desktop/..'
        if (!strcmp("..", name)){
            char *slash_ptr = strrchr(new_abs_path, '/');
            // 如果没有找到根目录的"/", 则截断
            if (slash_ptr != new_abs_path)
                *slash_ptr = 0;
            // 如果已经找到了根目录, 则截断为'/0xxxxx'
            else
                *(slash_ptr + 1) = 0;
        // 如果当前目录是当前目录
        } else if (strcmp(".", name)){
            if (strcmp(new_abs_path, "/"))
                strcat(new_abs_path, "/");
            strcat(new_abs_path, name);
        }

        // 继续遍历下一层目录
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (sub_path)
            sub_path = path_parse(sub_path, name);
    }
}


/**
 * @brief make_clear_abs_path用于将包含相对路径的目录path('.'和'..')处理不含相对路径的目录, 并存入final_path中
 * 
 * @param path 包含相对路径的目录
 * @param final_path 不包含相对路径的目录
 */
void make_clear_abs_path(char *path, char *final_path){
    char abs_path[MAX_PATH_LEN] = {0};

    if (path[0] != '/'){
        memset(abs_path, 0, MAX_PATH_LEN);
        if (getcwd(abs_path, MAX_PATH_LEN) != NULL)
            if (!(abs_path[0] == '/' && abs_path[1] == 0))
                strcat(abs_path, "/");
    }
    strcat(abs_path, path);
    wash_path(abs_path, final_path);
}


/**
 * @brief builtin_pwd是pwd内建命令的实现函数
 * 
 * @param argc 参数个数, 用不到, 统一格式用
 * @param argv 参数个数, 用不到, 统一格式用
 */
void builtin_pwd(uint32_t argc, char **argv __attribute__((unused))){
    if (argc != 1){
        printf("pwd: pwd receives no argument!\n");
        return;
    } else {
        if (NULL != getcwd(final_path, MAX_PATH_LEN))
            printf("%s\n", final_path);
        else
            printf("pwd: fail to get current work directory!\n");
    }
}


/**
 * @brief buildin_cd是cd内建命令的实现函数
 * 
 * @param argc 参数个数, cd命令支持一个参数
 * @param argv 参数个数, cd命令的参数要求是目标目录
 */
char *buildin_cd(uint32_t argc, char **argv){
    if (argc > 2){
        printf("cd: cd receieves no argument!\n");
        return NULL;
    }

    if (argc == 1){
        final_path[0] = '/';
        final_path[1] = 0;
    } else {
        make_clear_abs_path(argv[1], final_path);
    }

    if (chdir(final_path) == -1){
        printf("cd: no such directory %s\n", final_path);
        return NULL;
    }
    return final_path;
}


/**
 * @brief builin_ls是ls内置命令的实现函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 */
void builtin_ls(uint32_t argc, char **argv){
    char *pathname = NULL;
    stat_t file_stat;
    memset(&file_stat, 0, sizeof(stat_t));

    bool long_info = false;
    uint32_t arg_path_nr = 0;
    uint32_t arg_idx = 1;
    // 循环处理参数
    while (arg_idx < argc){
        // 处理命令行参数
        if (argv[arg_idx][0] == '-'){
            // -l 参数
            if (!strcmp("-l", argv[arg_idx])){
                long_info = true;
            // -h 参数
            } else if (!strcmp("-h", argv[arg_idx])){
                printf(
                    "ls: list all file in current working directory (cwd). Wish builtin command\n"
                    "Usage:\n"
                    "    -l     list all infomation about the file.\n"
                    "    -h     show this help message.\n"
                );
            } else {
                printf("ls: invalid option %s. Run `ls -h` for more infomation.\n");
            }
        // ls 路径参数
        } else {
            if (arg_path_nr == 0){
                pathname = argv[arg_idx];
                arg_path_nr = 1;
            } else {
                printf("ls: ls only receives 1 path!\n");
                return;
            }
        }
        arg_idx++;
    }


    // 开始真正切换目录
    if (pathname == NULL){
        if (NULL != getcwd(final_path, MAX_PATH_LEN)){
            pathname = final_path;
        } else {
            printf("ls: getcwd for default path failed!\n");
            return;
        }
    } else {
        make_clear_abs_path(pathname, final_path);
        pathname = final_path;
    }

    if (stat(pathname, &file_stat) == -1){
        printf("ls: cannot access %s: No such file or directory!\n", pathname);
        return;
    }

    // ls 目录, 则循环遍历目录下所有的文件
    if (file_stat.st_filetype == FT_DIRECTORY){
        dir_t *dir = opendir(pathname);
        dir_entry_t *de = NULL;
        char sub_pathname[MAX_PATH_LEN] = {0};
        uint32_t pathname_len = strlen(pathname);
        uint32_t last_char_idx = pathname_len - 1;
        memcpy(sub_pathname, pathname, pathname_len);

        if (sub_pathname[last_char_idx] != '/'){
            sub_pathname[pathname_len] = '/';
            pathname_len++;
        }

        rewinddir(dir);

        if (long_info){
            char ftype;
            printf("total: %d\n", file_stat.st_size);
            while ((de = readdir(dir))){
                ftype = 'd';
                if (de->f_type == FT_REGULAR)
                    ftype = '-';
                
                sub_pathname[pathname_len] = 0;
                strcat(sub_pathname, de->filename);
                memset(&file_stat, 0, sizeof(stat));

                if (stat(sub_pathname, &file_stat) == -1){
                    printf("ls: cannot access to %s: No such file or directory!\n", de->filename);
                    return;
                }

                printf("%c  %d  %d  %s\n", ftype, de->i_no, file_stat.st_size, de->filename);
            }
        } else {
            while ((de = readdir(dir)))
                printf("%s  ", de->filename);
            printf("\n");
        }
        closedir(dir);
    
    // ls 文件, 输出即可
    } else {
        if (long_info)
            printf("-  %d  %d  %s\n", file_stat.st_ino, file_stat.st_size, pathname);
        else
            printf("%s\n", pathname);
    }
}


/**
 * @brief builtin_ps是ps内置命令的实现函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 */
void builtin_ps(uint32_t argc, char **argv __attribute__((unused))){
    if (argc != 1){
        printf("ps: ps receives no argument!\n");
        return;
    }
    ps();
}


/**
 * @brief builtin_clear是clear系统调用的实现函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 */
void builtin_clear(uint32_t argc, char **argv __attribute__((unused))){
    if (argc != 1){
        printf("clear: clear receives no argument!\n");
        return;
    }
    clear();
}

/**
 * @brief builtin_mkdir是mkdir命令的内建函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 * @return int32_t 若创建成功, 则返回0; 若创建失败, 则返回-1
 */
int32_t builtin_mkdir(uint32_t argc, char **argv){
    int32_t ret = -1;
    if (argc != 2) {
        printf("mkdir: mkdir receives only 1 argument!\n");
    } else {
        make_clear_abs_path(argv[1], final_path);
        // 创建的不是根目录
        if (strcmp("/", final_path)){
            if (mkdir(final_path) == 0){
                ret = 0;
            } else {
                printf("mkdir: create directory %s failed!\n", argv[1]);
            }
        }
    }
    return ret;
}


/**
 * @brief builtin_rmdir是rmdir的内建函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t builtin_rmdir(uint32_t argc, char **argv){
    int32_t ret = -1;
    if (argc != 2){
        printf("rmdir: rmdir only receives 1 argument!\n");
    } else {
        make_clear_abs_path(argv[1], final_path);
        // 不能删除根目录
        if (strcmp("/", final_path)){
            if (rmdir(final_path) == 0)
                ret = 0;
            else
                printf("rmdir: remove %s failed.\n", argv[1]);
        }
    }
    return ret;
}


/**
 * @brief builtin_rm是rm内置命令的实现函数
 * 
 * @param argc 参数个数
 * @param argv 参数值
 * @return int32_t 若删除成功, 则返回0; 若删除失败, 则返回-1
 */
int32_t builtin_rm(uint32_t argc, char **argv){
    int32_t ret = -1;
    if (argc != 2){
        printf("rm: rm receives only 1 argument!\n");
    } else {
        make_clear_abs_path(argv[1], final_path);
        // 删除的不是根目录
        if (strcmp("/", final_path)){
            if (unlink(final_path) == 0)
                ret = 0;
        } else {
            printf("rm: delet %s failed!\n", argv[1]);
        }
    }

    return ret;
}


int32_t builtin_touch(uint32_t argc, char **argv){
    int32_t ret = -1;
    if (argc != 2) {
        printf("touch: touch receives only 1 argument!\n");
    } else {
        char temp[MAX_PATH_LEN] = {0};
        char filepath[MAX_PATH_LEN] = {0};
        getcwd(temp, MAX_PATH_LEN);
        strcat(temp, argv[1]);
        make_clear_abs_path(temp, filepath);
        // 创建的不是根目录
        if (strcmp("/", final_path)){
            if (open(final_path, O_CREAT) != -1){
                ret = 0;
            } else {
                printf("touch: create file %s failed!\n", argv[1]);
            }
        }
    }
    return ret;
}