; 保护模式下运行的代码
[bits 32]

%define ERROR_CODE nop              ; 若在异常中CPU已经把错误代码压栈了，那么就不作操作
%define ZERO push 0                 ; 若在异常中CPU没有把错误代码压栈，那么手动压入0

extern idt_table

section .data

global intr_entry_table
intr_entry_table:

; 宏函数 
; -----------------------------------------------------------
; VECTOR  宏函数用于定义中断处理函数
; -----------------------------------------------------------
; 无输入:
;       %1 是中断号
;       %2 用于处理每个中断
; 无返回值:
%macro VECTOR 2
section .text
intr%1entry:
    ; 下买呢这部分push的内容和intr_stack_t中的内容对应
    %2                              ; 压入error_code
    ; 保存上下文
    push ds
    push es
    push fs
    push gs
    pushad                          ; 依次压入eax, ecx, edx, ebx, esp, ebp, esi, edi

    ; 主片和从片复位, 发送总段结束命令
    mov al, 0x20
    out 0xA0, al                    ; 从片复位
    out 0x20, al                    ; 主片复位

    push %1                         ; 传参，压入中断向量号
    call [idt_table + %1*4]         ; CS:IP在这里改变
    jmp intr_exit

; 汇编中的 section ，即节，用来定义一段相同属性的数据，该范围起始于当前 section 的定义处，
; 直持续到下一个 section 的定义处，若没有遇到新的 section 定义，则一直持续到文件结束处。
; 名字相同（属性相同）的段会被合并到一起
section .data
    dd intr%1entry                  ; 储存个个中断入口程序地址, 形成intr_entry_table数组

%endmacro

section .text
global intr_exit
; 函数 
; -----------------------------------------------------------
; intr_exit  退出中断处理函数时恢复上下文信息
; -----------------------------------------------------------
; 无输入:
; 无返回值:
intr_exit:
    add esp, 4                      ; 跳过中断号
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4                      ; 跳过error_code
    iretd


; 以下是Intel CPU内部会发出的中断号，因此在设置8259A的时候，不能用到前面这些
VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO 
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO 
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE 
VECTOR 0x0c,ZERO
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO 
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO 
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO 
VECTOR 0x18,ERROR_CODE
VECTOR 0x19,ZERO
VECTOR 0x1a,ERROR_CODE
VECTOR 0x1b,ERROR_CODE 
VECTOR 0x1c,ZERO
VECTOR 0x1d,ERROR_CODE
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO 

; 以下是用户编程的中断
VECTOR 0x20,ZERO                ; 时钟中断
VECTOR 0x21,ZERO                ; 键盘中断
VECTOR 0x22,ZERO                ; 级联保留
VECTOR 0x23,ZERO                ; 串口2对应的入口
VECTOR 0x24,ZERO                ; 串口1对应的入口
VECTOR 0x25,ZERO                ; 并口2对应的入口
VECTOR 0x26,ZERO                ; 软盘对应的入口
VECTOR 0x27,ZERO                ; 并口1对应的入口
VECTOR 0x28,ZERO                ; 实时时钟对应的入口
VECTOR 0x29,ZERO                ; 重定向
VECTOR 0x2A,ZERO                ; 保留
VECTOR 0x2B,ZERO                ; 保留
VECTOR 0x2C,ZERO                ; ps/2鼠标
VECTOR 0x2D,ZERO                ; fpu浮点异常
VECTOR 0x2E,ZERO                ; 硬盘
VECTOR 0x2F,ZERO                ; 保留

