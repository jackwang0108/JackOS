#include "stdio.h"
#include "string.h"
#include "syscall.h"


int main(void){
    int a = 0x20010107;
    printf("Hello World from first user_program on JackOS.\n");
    printf("My name is Jack, birthday: 0x%x\n", a);
    return 0;
}