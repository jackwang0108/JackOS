%include "boot.inc"

section loader  vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR

; 构建gdt及其内部描述符
GDT_BASE:
    dd  0x0000_0000
    dd  0x0000_0000
CODE_DESC:
    dd  0x0000_FFFF
    dd  DESC_CODE_HIGH4
DATA_STACK_DESC:
    dd  0x0000_FFFF
    dd  DESC_DATA_HIGH4
VIDEO_DESC:
    dd  0x8000_0007
    dd DESC_VIDEO_HIGH4
GDT_SIZE    equ     $ - GDT_BASE            ; GDT_SIZE和GDT_LIMIT都是为后续加载GDT做准备
GDT_LIMIT   equ     GDT_SIZE - 1
times 60 dq 0                               ; 为GDT预留60个描述符的位置

; total_mem bytes 用于保存内存容量，以字节为单位，此位置比较好记
; 当前偏移 loader.bin 文件头 Ox200 字节 (64个段描述符 * 8字节 + 0个jmp指令 * 3字节 = 515字节, 203H)
; loader. bin 的加载地址是 Ox900
; 故 total mem_bytes 内存中的地址是 OxbOO 将来在内核中咱们会引用此地址
total_mem_bytes dd 0

; 构建段选择子
SELECTOR_CODE   equ     (0x0001 << 3) + TI_GDT + RPL0
SELECTOR_DATA   equ     (0x0002 << 3) + TI_GDT + RPL0
SELECTOR_VIDEO  equ     (0x0003 << 3) + TI_GDT + RPL0

; GDT指针
gdt_ptr dw      GDT_LIMIT
    dd      GDT_BASE

; ards结构体数组, total_memory_byes(4 字节) + gdt_ptr(6 字节) + ards_buf(244 字节) + ards_nr (2 字节) = 256字节
ards_buf times 244 db 0
ards_nr dw 0

; data
loader_msg db 'Enter loader: Real Mode'

loader_start:

    ; 输出字符串 利用0x13功能
    ; -----------------------------------------------------------
    ; INT 0x10   功能号:0x13	   功能描述:输出字符串
    ; -----------------------------------------------------------
    ; 输入:
    ;   AH 功能号= 0x13
    ;   AL = 写字符方式, AL=0x01表示光标跟随字符移动(每写一个字符，光标后移一个字符)
    ;       - 0: 字符串中只含显示字符, 其显示属性在BL, 显示后，光标位置不变
    ;       - 1: 字符串中只含显示字符, 其显示属性在BL, 显示后，光标位置改变
    ;       - 2: 字符事中含显示字符和显示属性。显示后, 光标位置不变
    ;       - 3: 字符串中含显示字符和显示属性。显示后, 光标位置改变
    ;   BH = 要显示的页号
    ;   BL = 字符属性, BL=0x02表示黑底绿字
    ;   CX = 字符串的长度，不包括结尾的'\0'
    ;   DX = 光标的位置
    ;   ES:BP = 字符串首地址
    ; 无返回值:
    mov sp, LOADER_STACK_TOP
    mov bp, loader_msg
    mov cx, 23
    mov ax, 0x13_01
    mov bx, 0x00_1F         ; 1F: 蓝底粉红字
    mov dx, 0x01_00
    int 0x10

    ; 设置光标位置 利用0x02功能
    ; -----------------------------------------------------------
    ; INT 0x10   功能号:0x02	   功能描述:设置光标位置
    ; ------------------------------------------------------
    ; 输入:
    ;   AH 功能号= 0x02
    ;   BH = 待设置光标的页号
    ; 返回值:
    ;   (DH, DL) = 光标所在行号, 光标所在列号
    mov ah, 0x02
    mov dx, 0x05_00

    int 0x10

    ; 获取内存大小 利用0x15中断的E820功能号
    ; -----------------------------------------------------------
    ; INT 0x15   功能号: 0xE820	   功能描述: 以ARDS结构体形式返回内存大小
    ; -----------------------------------------------------------
    ; 输入:
    ;   EAX 功能号= 0xE820
    ;   EDX = SMAP签名，固定为0x534D4150，当成magic number处理
    ;   ECX = ARDS结构的字节大小，目前是20字节
    ;   EBX = 后续值，由BIOS填写，初次用要设置为0
    ;   ES:DI = ARDS缓冲区的地址
    ; 返回值:
    ;   CF位: 0表示调用为出错，1表示出错
    ;   EAX: SMAP签名
    ;   ECX: 写入的字节大小
    ;   EBX: 后续值
    ;   ES:DI = 和输入值一样，但是已经填充了信息
    xor ebx, ebx
    mov edx, 0x534D4150
    mov di, ards_buf
    .e820_mem_get_loop:
        mov eax, 0x0000_E820
        mov ecx, 20
        int 0x15
        jc .e820_failed_so_try_e801     ; CF=1，有错误发生，则尝试使用e801获取内存
        add di, cx                      ; 移动指针
        inc word [ards_nr]              ; ards数量加1
        cmp ebx, 0                      ; 最后一个结构，返回值为0
        jnz .e820_mem_get_loop

    ; 获取ards数组中BaseAddrLow + LengthLow最大值，就是寻址空间内存的大小
    mov cx, [ards_nr]
    mov ebx, ards_buf                   ; ebx是指向ards的指针
    xor edx, edx
    .find_max_mem_area:
        mov eax, [ebx]
        add eax, [ebx + 8]
        add ebx, 20                     ; 指向下一个ards指针
        cmp edx, eax
        jge .next_ards                  ; edx <= eax
        mov edx, eax
        .next_ards:
            loop .find_max_mem_area
            jmp .mem_get_ok


    ; 通过BIOS 15H中断的E820功能号失败，则尝试E801功能号获取内存大小
    .e820_failed_so_try_e801:

    ; 获取内存大小 利用0x15中断的E801功能号
    ; -----------------------------------------------------------
    ; INT 0x15   功能号: 0xE801	   功能描述: 底端在AX/CX，高端在BX/DX
    ; -----------------------------------------------------------
    ; 输入:
    ;   EAX 功能号= 0xE801
    ; 返回值:
    ;   CF位: 0表示调用为出错，1表示出错
    ;   AX: 低端15MB的寻址空间大小，以1KB为单位
    ;   CX: 和AX相同
    ;   BX: 高端16MB~4GB的寻址空间大小，以64KB为单位
    ;   DX: 和BX相同
    mov eax, 0xE801
    int 0x15
    jc .e801_failed_so_try88            ; CF=1，有错误发生，尝试使用0x88功能号

    ; 首先计算底端16MB内存大小
    mov cx, 0x400                       ; 1KB = 2^10B
    mul cx                              ; ax * cx, 16位计算结果32位在DX:AX
    shl edx, 16
    and eax, 0x0000_FFFF
    or edx, eax
    add edx, 0x10_0000                  ; 高端1MB保留给ISA了
    mov esi, edx

    ; 然后计算16MB以上的内存
    xor eax, eax
    mov ax, bx
    mov ecx, 0x1_0000
    mul ecx                             ; eax * ecx, 32位计算结果64位EDX:EAX
    add esi, eax
    mov edx, esi

    jmp .mem_get_ok


    ; 通过BIOS 15H中断的E801功能号失败，则尝试88功能号获取内存大小
    .e801_failed_so_try88:

    ; 获取内存大小 利用0x15中断的88功能号
    ; -----------------------------------------------------------
    ; INT 0x15   功能号: 0x88	   功能描述: 底端在AX/CX，高端在BX/DX
    ; -----------------------------------------------------------
    ; 输入:
    ;   AH 功能号= 0x88
    ; 返回值:
    ;   CF位: 0表示调用为出错，1表示出错
    ;   AX: 以1KB作为单位大小的1MB以上的连续单位数量
    mov ah, 0x88
    int 0x15
    jc .err_hlt                         ; CF=1，有错误发生，此时三种方式都无法获得寻址空间大小，停机报错
    and eax, 0x0000_FFFF

    mov cx, 0x400
    mul cx                              ; ax * cx, 16位计算结果32位在DX:AX
    shl edx, 16
    or edx, eax
    add edx, 0x10_0000

    ; 将寻址空间大小存入total_mem_bytes处
    .mem_get_ok:
        mov [total_mem_bytes], edx
    

    ; -------------------- 准备进入保护模式 --------------------
    ; 第一步：打开A20
    ; 第二步：加载GDT
    ; 第三步：cr0的pe位设置为1

    ; 第一步: 打开A20
    in al, 0x92
    or al, 0000_0010B
    out 0x92, al

    ; 第二步: 记载GDT
    lgdt [gdt_ptr]

    ; 第三步: cr0第0位置1
    mov eax, cr0
    or eax, 0x0000_0001
    mov cr0, eax

    ; 第四步: 刷新流水线
    jmp dword SELECTOR_CODE:p_mode_start

    .err_hlt:
        hlt

[bits 32]
p_mode_start:                           ; 保护模式开启，此后所有代码都是运行在保护模式下
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LOADER_STACK_TOP
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    mov byte [gs:320], 'P'
    mov byte [gs:322], 'r'
    mov byte [gs:324], 'o'
    mov byte [gs:326], 't'
    mov byte [gs:328], 'e'
    mov byte [gs:330], 'c'
    mov byte [gs:332], 't'
    mov byte [gs:334], 'i'
    mov byte [gs:336], 'o'
    mov byte [gs:338], 'n'
    mov byte [gs:340], ' '
    mov byte [gs:342], 'm'
    mov byte [gs:344], 'o'
    mov byte [gs:346], 'd'
    mov byte [gs:348], 'e'

    ; -------------------- 加载内核 --------------------
    mov eax, KERNEL_START_SECTOR
    mov ebx, KERNEL_BIN_BASE_ADDR
    mov ecx, 200

    call rd_disk_m_32

    ; -------------------- 准备开启内存分页 --------------------
    ; 第一步：准备页目录表、页表
    ; 第二步：段描述符表中线性地址转虚拟地址
    ; 第三步：页目录地址赋给cr3
    ; 第四步：cr0的pg位设置为1
    ; 第五步：分页机制下重新加载段描述符表

    ; 第一步：准备页目录表、页表
    call setup_page

    ; 第二步：线性地址转虚拟地址, 因为等会开启分页后，线性地址就成虚拟地址了，所以为了开启分页之后还是能够访问, 这里先把前面的线性地址转为虚拟地址
    sgdt [gdt_ptr]                              ; 段描述符表地址和大小先保存，等会要重新加载
    mov ebx, [gdt_ptr + 2]                      ; 首先转换视频段
    or dword [ebx + 0x18 + 4], 0xC000_0000      ; 第三个段描述符指向视频段，每个段描述符8字节，所以 + 0x18, 高4字节是段基地址的31~24位
    add dword [gdt_ptr + 2], 0xC000_0000        ; gdt也要转换
    add esp, 0xC000_0000                        ; 栈也要转换

    ; 第三步：页目录地址赋给cr3
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax

    ; 第四步：cr0的pg位设置为1
    mov eax, cr0
    or eax, 0x8000_0000
    mov cr0, eax

    ; 第五步：分页机制下重新加载段地址
    lgdt [gdt_ptr]

    mov byte [gs:480], 'V'
    mov byte [gs:482], 'i'
    mov byte [gs:484], 'r'
    mov byte [gs:486], 't'
    mov byte [gs:488], 'u'
    mov byte [gs:490], 'a'
    mov byte [gs:492], 'l'
    mov byte [gs:494], ' '
    mov byte [gs:496], 'm'
    mov byte [gs:498], 'o'
    mov byte [gs:500], 'd'
    mov byte [gs:502], 'e'

    ; 以防万一, 强制刷新流水线
    jmp SELECTOR_CODE:enter_kernel

    enter_kernel:
        ; 生成内核的内存映像
        call kernel_init
        ; 未来要使用PCB来管理系统内核的栈，而一个PCB将占用一个页
        ; 因此未来内核将占用0xC009E000这个页，所以此时栈顶指针就将指向0xC0097000
        ; 为内核准备栈, 0xC009F000是1M下高608KB空闲空间，所以肯定够用
        mov esp, 0xC009F000
        mov byte [gs:640], 'J'
        mov byte [gs:642], 'u'
        mov byte [gs:644], 'm'
        mov byte [gs:646], 'p'
        mov byte [gs:648], ' '
        mov byte [gs:650], 't'
        mov byte [gs:652], 'o'
        mov byte [gs:654], ' '
        mov byte [gs:656], 'K'
        mov byte [gs:658], 'e'
        mov byte [gs:660], 'r'
        mov byte [gs:662], 'n'
        mov byte [gs:664], 'e'
        mov byte [gs:666], 'l'
        jmp KERNEL_ENTRY_POINT



; 函数 
; -----------------------------------------------------------
; setup_page   初始化页目录和第一个页表
; -----------------------------------------------------------
; 无输入:
; 无返回值:
setup_page:
    ; 清除页目录的内存
    mov ecx, 4096
    mov esi, 0
    .clear_page_dir:                    ; 逐字节清除
        mov byte [PAGE_DIR_TABLE_POS + esi], 0
        inc esi
        loop .clear_page_dir
    
    ; 创建页目录项
    .create_pde:
        mov eax, PAGE_DIR_TABLE_POS     ; PAGE_DIR_TABLE_POS = 0x10_0000
        add eax, 0x1000                 ; 0x1000 = 4096, 页目录占用一个页，后面的就是下一个页
        mov ebx, eax                    ; 第一个页的地址（不计页目录的也）eax = 0x10_1000
        
        or eax, PG_US_U | PG_RW_W | PG_P        ; 构建页目录项
        mov [PAGE_DIR_TABLE_POS + 0x0], eax     ; 第一个页目录项，指向的页表在0x10_1000，这个页表是第一个页，也是操作系统所在的页
        mov [PAGE_DIR_TABLE_POS + 0xc00], eax   ; 0xc00就是第768个页表, 让第768个页表也指向操作系统的页表,0~767一共768个页表，表示3G空间; 768~1022一个254个页表，表示3G-4M空间

        sub  eax, 0x1000                        ; eax = 0x10_0000, 指向页目录地址本身
        mov [PAGE_DIR_TABLE_POS + 4092], eax    ; 页目录表最后一个页目录项指向页目录自己（页目录占用一个页）
    
    ; 创建第一个页表(0x10_1000)的内容，这个页表指向操作系统
    mov ecx, 256                            ; 一个页表4M, 操作系统在0~1M, 一个页4K, 所以表示操作系统需要256个页
    mov esi, 0
    xor edx, edx
    mov edx, PG_US_U | PG_RW_W | PG_P       ; edx = 0x0000_0000, 指向操作系统
    .create_pte:
        mov [ebx + esi * 4], edx            ; ebx = 0x10_1000，第一个页的地址
        add edx, 4096                       ; edx + 4096, 下一个页
        inc esi
        loop .create_pte
    
    ; 创建内核其他页表的PDE
    mov eax, PAGE_DIR_TABLE_POS             ; PAGE_DIR_TABLE_POS = 0x10_0000
    add eax, 0x2000                         ; 第二个页的地址（不计页目录的页）eax = 0x10_2000
    or eax, PG_US_U | PG_RW_W | PG_P        ; 构建页目录项
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx, 254
    mov esi, 769
    .create_kernel_pde:
        mov [ebx + esi * 4], eax            ; ebx是Page_dir的地址
        inc esi
        add eax, 0x1000                     ; 0x1000 = 4096，下一个页
        loop .create_kernel_pde

    ret


; 函数 
; -----------------------------------------------------------
; kernel_init  用于从硬盘中读取操作系统内核(kernel.bin)，并将其中的段拷贝到虚拟地址0xc0001500处
; 函数调用前需要先调用 rd_disk_m_32把内核读取到KERNEL_BIN_BASE_ADDR处
; -----------------------------------------------------------
; 无输入:
; 无返回值:
kernel_init:
    xor eax, eax
    xor ebx, ebx                            ; ebx将用于记录程序头标地址
    xor ecx, ecx                            ; ex将用于记录程序头表中program header的数量
    xor edx, edx                            ; dx将用于记录program header的大小

    ; 调用kernel_init前, 需要先调用rd_disk_m_32把kernel.bin读取到KERNEL_BIN_BASE_ADDR处
    mov dx, [KERNEL_BIN_BASE_ADDR + 42]     ; ELF格式偏移文件42字节处是e_phentsize, 表示program header大小
    mov ebx, [KERNEL_BIN_BASE_ADDR + 28]    ; ELF格式偏移文件28字节处是e_phoff, 表示程序头表在文件中的偏移量

    add ebx, KERNEL_BIN_BASE_ADDR           ; 此时ebx指向程序头表
    mov cx, [KERNEL_BIN_BASE_ADDR + 44]     ; ELF格式偏移文件44字节处是e_phnum, 表示有及格Program header
    
    ; 前面都没问题，BUG 这里有问题，复制的不对，cx有问题
    .each_segment:
        ; 检查是否为无效段
        cmp byte [ebx + 0], PT_NULL
        je .PT_NULL                         ; 无效段，复制下一个段

        ; 函数原型: memcpy(dst, src, size)
        ; 为函数压入参数, 从右向左, 先压入size, 然后是src, 最后是dst
        push dword [ebx + 16]               ; program header中偏移16字节的地方时p_filesz, 压入size参数
        mov eax, [ebx + 4]                  ; program header中偏移4字节的是p_offset
        add eax, KERNEL_BIN_BASE_ADDR       ; 加上程序被加载到的地址, 则eax位该段的物理地址
        push eax                            ; 压入src参数
        push dword [ebx + 8]                ; 压入dst参数, 程序头表偏移8字节处为程序其实的虚拟地址
        call mem_cpy                        ; 将该段拷贝到虚拟地址对应的地址上去
        add esp, 12                         ; 3 * 4 = 12字节, 清除三个参数

        .PT_NULL:
        add ebx, edx                        ; edx是每个program header的大小, 让ebx指向下一个program header
        loop .each_segment
    
    ; BUG: 运行不到这句话
    ret


; 函数 
; -----------------------------------------------------------
; mem_cpy  用于src中拷贝size个字节到dst指向的内存区域
; -----------------------------------------------------------
; 无输入:
; 无返回值:
mem_cpy:
    cld
    ; 保存寄存器
    push ebp
    mov ebp, esp
    push ecx

    ; call时候保存的ecs:eip占用了 2 * 4 = 8 个字节
    mov edi, [ebp + 8]                      ; dst
    mov esi, [ebp + 12]                     ; src
    mov ecx, [ebp + 16]                     ; size
    rep movsb                               ; 逐字节拷贝, 用到了ecx、edi、esi
    ; .copy_btye:
    ;     mov ax, byte [esi]
    ;     mov byte [edi], ax
    ;     inc esi
    ;     inc edi
    ;     loop .copy_btye

    ; 恢复寄存器
    pop ecx
    pop ebp
    ret


; 函数 
; -----------------------------------------------------------
; rd_disk_m_32   32位模式下读取硬盘n个扇区
; ------------------------------------------------------
; 输入:
;   EAX = 要读取的扇区的LBA扇区号
;   BX = 数据将写入的地址
;   CX = 读取的扇区数
; 无返回值:
rd_disk_m_32:	   
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
        mov [ebx], ax           ; 32位，需要修改寄存器从bx到ebx
        add ebx, 2
        loop .go_on_read
    
    ret

; 由于在实模式下偏移地址为16位,所以用bx只会访问到0~FFFFh的偏移。
; loader的栈指针为0x900,bx为指向的数据输出缓冲区,且为16位，
; 超过0xffff后,bx部分会从0开始,所以当要读取的扇区数过大,待写入的地址超过bx的范围时，
; 从硬盘上读出的数据会把0x0000~0xffff的覆盖，
; 造成栈被破坏,所以ret返回时,返回地址被破坏了,已经不是之前正确的地址,
; 故程序出会错,不知道会跑到哪里去。
; 所以改为ebx代替bx指向缓冲区,这样生成的机器码前面会有0x66和0x67来反转。
; 0X66用于反转默认的操作数大小! 0X67用于反转默认的寻址方式.
; cpu处于16位模式时,会理所当然的认为操作数和寻址都是16位,处于32位模式时,
; 也会认为要执行的指令是32位.
; 当我们在其中任意模式下用了另外模式的寻址方式或操作数大小(姑且认为16位模式用16位字节操作数，
; 32位模式下用32字节的操作数)时,编译器会在指令前帮我们加上0x66或0x67，
; 临时改变当前cpu模式到另外的模式下.
; 假设当前运行在16位模式,遇到0X66时,操作数大小变为32位.
; 假设当前运行在32位模式,遇到0X66时,操作数大小变为16位.
; 假设当前运行在16位模式,遇到0X67时,寻址方式变为32位寻址
; 假设当前运行在32位模式,遇到0X67时,寻址方式变为16位寻址.

