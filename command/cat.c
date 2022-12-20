#include "stdio.h"
#include "syscall.h"
#include "string.h"


int main(int argc, char **argv){
    if (argc > 2 || argc == 1){
        printf("cat: cat reveives only 1 argument!\n");
        exit(-2);
    }

    int buf_size = 1024;
    char abs_path[512] = {0};
    void *buf = malloc(buf_size);
    if (buf == NULL){
        printf("cat: malloc for buf failed!\n");
        return -1;
    }

    if (argv[1][0] != '/'){
        getcwd(abs_path, 512);
        strcat(abs_path, "/");
        strcat(abs_path, argv[1]);
    } else {
        strcpy(abs_path, argv[1]);
    }

    int fd = open(abs_path, O_RDONLY);
    if (fd == -1){
        printf("cat: open %s failed!\n", abs_path);
        return -1;
    }

    int read_bytes = 0;
    while (1){
        read_bytes = read(fd, buf, buf_size);
        if (read_bytes == -1)
            break;
        write(1, buf, buf_size);
    }
    free(buf);
    close(fd);
    return 66;
}