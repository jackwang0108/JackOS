#include "stdio.h"
#include "string.h"
#include "stdint.h"
#include "syscall.h"

int main(int argc, char* argv[]){
    int32_t ret = -1;
    char touch_path[512] = {0};
    uint32_t max_path_len = 512;
    if (argc != 2) {
        printf("touch: touch needs 2 argument but only receives 1 argument!\n");
    } else {
        getcwd(touch_path, max_path_len);
        uint32_t len = strlen(touch_path);
        if (touch_path[len - 1] != '/')
            touch_path[len] = '/';
        strcat(touch_path, argv[1]);
        
        // 创建的不是根目录
        if (strcmp("/", touch_path)){
            int32_t fd = open(touch_path, O_CREAT | O_RDWD);
            if (fd != -1){
                ret = 0;
            } else {
                printf("touch: create file %s failed!\n", argv[1]);
            }
            write(fd, " ", 1);
            close(fd);
        }
    }
    return ret;
}