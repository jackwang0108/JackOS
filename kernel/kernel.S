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
    ; 下面这部分push的内容和intr_stack_t中的内容对应, 保存中断发生时候的上下文
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

    ; [idt_table + %1*4]的通用格式
    push %1                         ; 压入中断向量号, 依旧是intr_stack_t的一部分
    call [idt_table + %1*4]         ; CS:IP在这里改变
    ; idt_table中的硬中断处理函数统一格式都是void xxxx_interrput_handler(void), 所以没有传入参数, 因此也不需要清理
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



[bits 32]
extern syscall_table

section .text
global syscall_handler
; 函数 
; -----------------------------------------------------------
; syscall_handler  系统调用的int 0x80软中断的入口函数
; -----------------------------------------------------------
;   注意, 该函数编译后DPL0是系统代码段, 因此使用的栈是tss肿的ss0:esp0
;   但是调用该函数的是DPL3的用户代码, 所以用户通过DPL3的系统调用中断门
;   0x80 访问到这里之后, 就会发生一个DPL3 -> DPL0的转换, 这个时候
;   CPU会自动发生栈的切换, 从3级栈切换到0级栈
;
;   此外, jmp int_exit的时候, 会把压入的pushad, gs, fs, es, ds
;   全部弹出, 所以eax中的返回值会被覆盖, 因此需要将其保存到栈中,
;   但是注意, 由于jmp intr_exit中道ret之前都是DPL0, 所以此时eax被
;   保存到了0级栈中
; -----------------------------------------------------------
; 输入:
;       eax: 系统调用号
; 无返回值:
syscall_handler:
    ; 下面这部分push的内容和intr_stack_t中的内容对应, 保存中断发生时候的上下文
    push 0                      ; 给error_code压入0
    push ds
    push es
    push fs
    push gs
    pushad                      ; 依次压入eax, ecx, edx, ebx, esp, ebp, esi, edi

    ; 因为是CPU内部发出的软中断, 所以不需要像intr%1entry处理8259A一样先对仲裁器进行复位
    push 0x80                   ; 压入中断向量号, 依旧是intr_stack_t的一部分
    push edx                    ; 传入第3个参数
    push ecx                    ; 传入第2个参数
    push ebx                    ; 传入第1个参数
    call [syscall_table + eax * 4]
    add esp, 12                 ; 传了三个参数清理栈

    ; 系统调用的 [syscall_table + eax * 4](arg1, arg2, arg3) 的返回值将存入eax中
    mov [esp + 8 * 4], eax      ; 这个是二进制编程接口abi的约定
    jmp intr_exit