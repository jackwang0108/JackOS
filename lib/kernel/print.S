; 构建段选择子
TI_GDT          equ 0
RPL0            equ 0
SELCTOR_VIDEO   equ (0x0003<<3) + TI_GDT + RPL0

section .data
    put_int_buffer dq 0

; 需要在保护模式下运行，因此都是32位的代码了
[bits 32]
section .text
global put_int
    ; 函数 
    ; -----------------------------------------------------------
    ; put_int 以16进制输出小端序的一个数字, 前面没有前缀
    ; -----------------------------------------------------------
    ; 输入:
    ;       int num
    ; 无返回值:
put_int:
    pushad
    mov ebp, esp
    mov eax, [ebp + 4 * 9]      ; call返回地址4字节 + 8个4字节
    mov edx, eax
    mov edi, 7                  ; 指定在put_int_buffer中的初始偏移量
    mov ecx, 8                  ; 32位系统一个int 4字节，8个十六进制数
    mov ebx, put_int_buffer

    ; int 转 ASCII
    .16based_4bits:
        and edx, 0x0000_000F
        cmp edx, 9
        jg .is_A2F
        add edx, '0'            ; 0~9，转为ASCII然后保存
        jmp .store
        .is_A2F:                ; A-F，加Az转为ASCII然后保存
            sub edx, 10
            add edx, 'A'
        .store:                 ; 存储当前16进制数字
            mov [ebx + edi], dl
            dec edi
            shr eax, 4
            mov edx, eax
        loop .16based_4bits
    
    ; 打印
    .ready_to_print:
        inc edi                 ; loop的条件是ecx > 0, 最后一次loop后ecx=0, 则edi=0
    
    .skip_prefix_0:             ; 0x000123 -> 0x123，找到第一个不为0的数字
        ; 比较到第九个字符而且都是0
        cmp edi, 8
        je .full0
        .go_on_skip:
            mov cl, [put_int_buffer + edi]
            inc edi
            cmp cl, '0'
            je .skip_prefix_0
            dec edi
            jmp .put_each_num
    
    .full0:
        mov cl, '0'
    .put_each_num:
        push ecx
        call put_char
        add esp, 4
        inc edi
        mov cl, [put_int_buffer + edi]
        cmp edi, 8
        jl .put_each_num

    popad
    ret

global put_str
    ; 函数 
    ; -----------------------------------------------------------
    ; put_str 输出以0结尾的字符串
    ; -----------------------------------------------------------
    ; 输入:
    ;       char* msg
    ; 无返回值:
put_str:
    push ebx
    push ecx

    xor ecx, ecx
    mov ebx, [esp + 12]         ; 字符串首地址
    .goon:
        mov cl, [ebx]
        cmp cl, 0
        jz .str_over
        push ecx                ; 给put_char传参
        call put_char
        add esp, 4              ; 清理栈，C语言使用的cstdl标准自动帮我们清理
        inc ebx
        jmp .goon
    .str_over:
    pop ecx
    pop ebx
    ret


global put_char
    ; 函数 
    ; -----------------------------------------------------------
    ; put_char 输出栈中的一个字符
    ; -----------------------------------------------------------
    ; 输入:
    ;       char str
    ; 无返回值:
put_char:
    pushad                      ; 保存所有寄存器
    ; 段选择子赋值
    mov ax, SELCTOR_VIDEO
    mov gs, ax

    ; 由于32位了，所以不能使用16位实模式下BIOS提供的中断了
    ; 需要手动访问显卡的寄存器来实现：获得光标位置、写字符……
    
    ; 获取光标位置
    ; 先向索引寄存器中写入0x0E，表示将要读取光标高八位
    mov dx, 0x03D4              ; 0x03D4 Address Register
    mov al, 0x0E                ; 光标高八位的索引
    out dx, al
    mov dx, 0x03D5              ; 0x03D5 Data Register
    in al, dx
    mov ah, al

    ; 再向索引寄存器中写入0x0F, 表示将要读取光标低8位
    mov dx, 0x03D4
    mov al, 0x0F
    out dx, al
    mov dx, 0x03D5
    in al, dx

    ; 光标存入bx中
    mov bx, ax

    ; 获取待打印的字符
    mov ecx, [esp + 36]      ; pushad 压入了eax, ebx, ecx, edx, esp, ebp, esi, edi八个4字节

    cmp cl, 0xd                 ; 0xd = CR, Retuen
    jz .is_carriage_return
    cmp cl, 0xa                 ; 0xa = LF, 
    jz .is_line_feed
    cmp cl, 0x8                 ; 0x8 = BS
    jz .is_backspace
    jmp .put_other


    ; 输出退格, 退格本质就是用一个空白覆盖前一个字符
    .is_backspace:
        dec bx
        shl bx, 1               ; 光标不包含属性，所以*2得到内存偏移
        mov byte [gs:bx], 0x20  ; 空格
        inc bx
        mov byte [gs:bx], 0x07
        shr bx, 1
        jmp .set_cursor

    ; 正常输出字符
    .put_other:
        shl bx, 1               ; 计算内存偏移
        mov [gs:bx], cl
        inc bx
        mov byte [gs:bx], 0x07
        shr bx, 1
        inc bx

        ; 分辨率80列*25行，一屏2000个字
        cmp bx, 2000
        jl .set_cursor
    
    
    ; 均视为换行
    .is_line_feed:
    .is_carriage_return:
        xor dx, dx              ; 除法高16位在dx
        mov ax, bx              ; ax是除数低16位
        mov si, 80
        div si
        sub bx, dx              ; 光标值减除以80得到的余数得到当前行数

        .is_carriage_return_end:
            add bx, 80          ; 下一行
            cmp bx, 2000
        .is_line_feed_end:
            jl .set_cursor

    ; 显存32KB，但是只使用一个页，所以丢弃第0行，第1~24行覆盖到第0~23行
    .roll_screen:
        cld
        mov ecx, 960            ; 需要复制 2000 - 80(第0行) = 1920个字符，3840个字节

        mov esi, 0xB80A0        ; 0xC00B8000 + 80 * 2 = 第1行行首
        mov edi, 0xB8000        ; 第0行行首
        ; 为什么会发生缺页中断? 
        ; movsd dword ptr es:[edi], dword ptr ds:[esi]
        rep movsd               ; 重复 3840字节 / 4字节 = 960次

        ; 最后一行填充为空白
        mov ebx, 3840
        mov ecx, 80
        .cls:
            mov word [gs:ebx], 0x0720               ; 0x20=空格，0x07=黑底白字
            add ebx, 2
            loop .cls
        
        ; 光标重置为最后一行行首
        mov bx, 1920
    
    ; 设置光标
    .set_cursor:
        ; 先设置高8位
        mov dx, 0x03D4          ; Address Register
        mov al, 0x0E            ; 高八位
        out dx, al
        mov dx, 0x03D5          ; 写寄存器来设置光标位置
        mov al, bh
        out dx, al

        ; 然后设置低8位
        mov dx, 0x03D4          ; Address Register
        mov al, 0x0F            ; 高八位
        out dx, al
        mov dx, 0x03D5          ; 写寄存器来设置光标位置
        mov al, bl
        out dx, al
    
    .put_char_done:
        popad
    
    ret


global set_cursor
    ; 函数 
    ; -----------------------------------------------------------
    ; set_cursor 用于设置光标的位置
    ; -----------------------------------------------------------
    ; 输入:
    ;       int 光标的位置
    ; 无返回值:
set_cursor:
    ; 保存所有寄存器
    pushad
    ; 获取参数
    mov bx, [esp + 36]

    ; 先设置光标的高八位
    mov dx, 0x03D4			  ; 设置索引寄存器
    mov al, 0x0E		      ; 用于提供光标高8位的寄存器的索引值为0x0E
    out dx, al
    mov dx, 0x03D5			  ; 通过读写数据端口0x3d5来获得或设置光标位置 
    mov al, bh
    out dx, al

    ; 先设置光标的底八位，同上
    mov dx, 0x03D4
    mov al, 0x0F
    out dx, al
    mov dx, 0x03D5 
    mov al, bl
    out dx, al
    popad
    ret
