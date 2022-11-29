%include "boot.inc"
; 主引导程序
; ----------------------------------------
section MBR vstart=0x7c00
    ; 设置段寄存器
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov sp, 0x7c00
    ; 设置显卡内存地址
    mov ax, 0xb800
    mov gs, ax

    ; 清屏 利用0x06号功能，上卷全部行，则可清屏。
    ; -----------------------------------------------------------
    ; INT 0x10   功能号:0x06	   功能描述:上卷窗口
    ; ------------------------------------------------------
    ; 输入:
    ;   AH 功能号= 0x06
    ;   AL = 上卷的行数(如果为0,表示全部)
    ;   BH = 上卷行属性
    ;   (CL,CH) = 窗口左上角的(X,Y)位置
    ;   (DL,DH) = 窗口右下角的(X,Y)位置
    ; 无返回值:
    mov ax, 0x06_00
    mov bx, 0x07_00
    mov cx, 0x00_00
    mov dx, 0x18_4f

    int 0x10

    ; 写显存方式打印字符串, 4表示前景色为红色，A表示绿色背景闪烁
    mov byte [gs:0x00], 'F'
    mov byte [gs:0x01], 0x4A
    mov byte [gs:0x02], 'r'
    mov byte [gs:0x03], 0x4A
    mov byte [gs:0x04], 'o'
    mov byte [gs:0x05], 0x4A
    mov byte [gs:0x06], 'm'
    mov byte [gs:0x07], 0x4A
    mov byte [gs:0x08], ' '
    mov byte [gs:0x09], 0x4A
    mov byte [gs:0x0a], 'B'
    mov byte [gs:0x0b], 0x4A
    mov byte [gs:0x0c], 'I'
    mov byte [gs:0x0d], 0x4A
    mov byte [gs:0x0e], 'O'
    mov byte [gs:0x0f], 0x4A
    mov byte [gs:0x10], 'S'
    mov byte [gs:0x11], 0x4A
    mov byte [gs:0x12], ':'
    mov byte [gs:0x13], 0x4A
    mov byte [gs:0x14], ' '
    mov byte [gs:0x15], 0x4A
    mov byte [gs:0x16], 'I'
    mov byte [gs:0x17], 0x4A
    mov byte [gs:0x18], 'n'
    mov byte [gs:0x19], 0x4A
    mov byte [gs:0x1a], ' '
    mov byte [gs:0x1b], 0x4A
    mov byte [gs:0x1c], 'M'
    mov byte [gs:0x1d], 0x4A
    mov byte [gs:0x1e], 'B'
    mov byte [gs:0x1f], 0x4A
    mov byte [gs:0x20], 'R'
    mov byte [gs:0x21], 0x4A

    mov eax, LOADER_START_SECTOR
    mov bx, LOADER_BASE_ADDR
    mov cx, 4                   ; 读取4个扇区
    call rd_disk_m_16           ; 读取硬盘

    ; 程序悬停在此
    jmp LOADER_BASE_ADDR + 0x300 + 23


; 函数 
; -----------------------------------------------------------
; rd_disk_m_16   读取硬盘n个扇区
; ------------------------------------------------------
; 输入:
;   EAX = 要读取的扇区的LBA扇区号
;   BX = 数据将写入的地址
;   CX = 读取的扇区数
; 无返回值:
rd_disk_m_16:
    ; 备份寄存器
    mov esi, eax
    mov di, cx
    ; 第一步：设置要读取的扇区数
    mov dx, 0x1F2               ; 0x1F2端口存放主通道需要读取的扇区数，in/out指令，端口号在0～255之间，则端口号直接写在指令中，端口号大于255，则端口号通过DX寄存器间接寻址，即端口号应先放入DX中
    mov al, cl
    out dx, al
    
    mov eax, esi                ; 恢复寄存器

    ; 第二步：写入LBA地址
    mov dx, 0x1F3
    out dx, al                  ; 写入LBA7~0位

    mov cl, 8
    shr eax, cl
    mov dx, 0x1F4
    out dx, al                  ; 写入LBA15~8位

    shr eax, cl
    mov dx, 0x1F5
    out dx, al                  ; 写入LBA23~16位

    shr eax, cl
    and al, 0x0F                ; LBA27~24位
    or al, 0xE0                 ; LBA寻址方式、主盘读取
    mov dx, 0x1F6
    out dx, al

    ; 第三步：开始读取
    mov dx, 0x1F7
    mov al, 0x20
    out dx, al

    ; 第四步：循环等待读取完毕
    .not_ready:
        nop
        in al, dx               ; 读取寄存器状态
        and al, 0x88     ; 第四位1表示硬盘已经准备好数据传输，第七位1表示硬盘忙
        cmp al, 0x08
        jnz .not_ready
    
    ; 第五步：从0x1F0读取数据
    mov ax, di
    mov dx, 256                 ; 一个扇区512字节，但是一次读两个字
    mul dx                      ; 计算要读取的字节数，因为一次只能读两个字节
    mov cx, ax

    mov dx, 0x1F0               ; 循环读取
    .go_on_read:
        in ax, dx
        mov [bx], ax
        add bx, 2
        loop .go_on_read
    
    ret


times 510 - ($ - $$) db 0
    db 0x55, 0xaa
