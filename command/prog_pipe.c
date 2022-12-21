#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "syscall.h"

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))){
    int32_t fd[2] = {-1};
    pipe(fd);
    pid_t pid = fork();
    if (pid){
        close(fd[0]);
        char str[40] = {0};
        sprintf(str, "Hello from father, father pid: %d", getpid());
        write(fd[1], str, strlen(str));
        printf("Father process, pid: %d\n", getpid());
        int32_t status __attribute__((unused));
        wait(&status);
        return 8;
    } else {
        close(fd[1]);
        char buf[40] = {0};
        read(fd[0], buf, 40);
        printf("Child process, pid: %d\n", getpid());
        printf("Receiving buf from parent: \"%s\"\n", buf);
        return 9;
    }
}