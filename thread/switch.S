[bits 32]
section .text
global switch_to
; 函数 
; -----------------------------------------------------------
; switch_to  用于实现任务的切换，注意这里要内存栈的图参考thread.c中的kernel_thread
; void switch_to(task_struct_t *cur, task_struct_t *next);
; -----------------------------------------------------------
; 输入（栈中的顺序）:
;   next: 下一进程的PCB
;   cur: 当前进程PCB
; 无返回值:
switch_to:
    ; 保存当前进程的上下文信息, 下面这部分push内容和thread_stack_t中的内容对应
    push esi
    push edi
    push ebx
    push ebp
    ; 保存当前程序的栈顶指针到PCB中区
    mov eax, [esp+20]               ; 前面有压入了4*4=16个字节的数据，然后还有cs这里获取的是cur的地址
    mov [eax], esp                  ; eax是cur这个task_struct_t的指针，第一个元素就是cur的self_kstack，即栈顶指针esp的值

    ; 加载下一个进程的上下文信息
    mov eax, [esp + 24]             ; 前有有五个, 此时esp+24指向next
    mov esp, [eax]
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret