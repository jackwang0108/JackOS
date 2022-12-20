#include "stdio.h"
#include "string.h"
#include "syscall.h"


int main(int argc, char* argv[]){
    printf("argc: %d\n", argc);
    for (int arg_idx = 0; arg_idx < argc; arg_idx++)
        printf("argv[%d] = %s\n", arg_idx, argv[arg_idx]);

    int pid = fork();
    if (pid){
        int delay = 900000;
        while (delay--);
        printf("\nFather process, pid: %d\n", getpid());
        printf("Process List:\n");
        ps();
    } else {
        printf("Child process, pid: %d\n", getpid());
        printf("Run now\n");

        char abs_path[512] = {0};
        if (argv[1][0] != '/'){
            getcwd(abs_path, 512);
            strcat(abs_path, "/");
            strcat(abs_path, argv[1]);
            execv(abs_path, argv);
        } else {
            execv(argv[1], argv);
        }
    }

    return 0;
}