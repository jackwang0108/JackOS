#include "stdio.h"
#include "string.h"
#include "syscall.h"

int main(int argc, char* argv[]){
    if (argc == 0){
        printf("echo: echo needs at least 1 argument!\n");
        printf(
            "Usage:\n"
            "    echo something\n"
            "    echo something > file\n"
        );
        return -1;
    }

    // echo 
    if (argc == 1){
        printf("\n");
        return 0;
    }

    // echo something  ||  echo something > file
    int file_idx = -1;
    for (int i = 0; i < argc; i++){
        if (!strcmp(argv[i], ">")){
            file_idx = i+1;
            break;
        }
    }

    // redirect stdout to file
    int32_t fd = -1;
    if (file_idx != -1){
        // get file abs path
        char abs_path[512] = {0};

        if (argv[file_idx][0] != '/'){
            getcwd(abs_path, 512);
            uint32_t len = strlen(abs_path);
            if (abs_path[len - 1] != '/')
                abs_path[len] = '/';
            strcat(abs_path, argv[file_idx]);
        } else {
            strcpy(abs_path, argv[file_idx]);
        }

        // open file
        fd = open(abs_path, O_CREAT | O_RDWD);
        if (fd == -1){
            printf("echo: open file %s failed!\n", abs_path);
            return 0;
        }
    } else {
        fd = 1;
    }

    
    uint32_t args_to_print = file_idx == -1 ? argc : file_idx - 1;
    for (uint32_t i = 1; i < args_to_print; i++){
        write(fd, argv[i], strlen(argv[i]));
        write(fd, " ", 1);
    }
    write(fd, "\n", 1);

    // redirect to screen
    if (file_idx != 1){
        close(fd);
    }
    return 0;
}