[bits 32]
extern main
extern exit

section .text
global _start
; 函数 
; -----------------------------------------------------------
;   _start是链接器连接的时候默认链接的入口函数
; -----------------------------------------------------------
; 无输入:
; 无返回值:
_start:
    push ebx
    push ecx
    call main

    ; 将main的返回值传给exit
    push eax
    call exit           ; 事实上exit不会返回, 相当于exit(eax)