; Master Boot Record (MBR) program

%include "boot.inc"

section MBR vstart=0x7c00   ; vstart=0x7c00, nasm will make cs points to 0x7c00
    ; set register
    mov ax, cs
    mov ds, ax          ; data seg also start from 0x7c00
    mov es, ax          ; same for extra seg
    mov ss, ax          ; we will use a short memory before 0x7c00 as stack
    mov sp, 0x7c00
    mov fs, ax



    ;clear the screen using BIOS interrupt (BIOS just throw many messages)

    ;----------------------------------------------------------------------
    ;INT 0x10   function number: 0x06	   Description: roll up the window
    ;----------------------------------------------------------------------
    ;input：
    ;AH function number= 0x06
    ;AL = number of lines to roll up (0 to roll all rows)
    ;BH = properties of rolled rows
    ;(CL,CH) = (X,Y) Position of upper left cornor of the window
    ;(DL,DH) = (X,Y) Position of lower right cornor of the window
    ;No Returns:
    
    mov ah, 0x06        ; set function number
    mov al, 0           ; roll all rows (clear the screen)
    mov cl, 0           ; window size under text mode is 79 * 24
    mov ch, 0
    mov dl, 79
    mov dh, 24
    int 10h



    ;print hello information of JackOS, first read cursor position then write message

    ;----------------------------------------------------------------------
    ;INT 0x10   function number: 0x03	   Description: get cursor position
    ;----------------------------------------------------------------------
    ;input：
    ;AH function number= 0x03
    ;BH = get cursor position of which page (There is 4 pages)
    ;Returns:
    ;CH = start row number of cursor
    ;CL = end row number of cursor
    ;DH = row number of cursor
    ;DL = line number of cursor
    mov ah, 0x03
    mov bh, 0
    int 10h



    ;----------------------------------------------------------------------
    ;INT 0x10   function number: 0x13	   Description: print a string
    ;----------------------------------------------------------------------
    ;input：
    ;AH function number= 0x13
    ;AL = print mode
    ;   0x00 just print the string and cursor stay where it was
    ;   0x01 print the string and cursor follows (cursor will be at the end finally)
    ;   0x02 print string with property
    ;   0x03 0x02 + 0x01 
    ;CX = string length
    ;DH = row number of cursor
    ;DL = line number of cursor
    ;ES:BP = location of string
    ;BH = print string to which page
    ;BL = font & color property
    ;No Returns:
    mov ax, greeting    ; set bp
    mov bp, ax
    sub dh, 10
    add dl, 25
    mov cx, greeting_end - greeting
    mov ax, 0x1301      ; set AH and AL
    mov bx, 0x0C        ; set BH = 0 and BL = C (Light RED) https://en.wikipedia.org/wiki/BIOS_color_attributes
    int 0x10

    ; print information before read disk
    ; sub dh, 4           ; move cursor to next line
    add dh, 1
    mov ax, read_disk_prompt
    mov bp, ax
    mov cx, read_disk_prompt_end - read_disk_prompt
    mov ax, 0x1301
    mov bx, 0x0c
    int 0x10

    ; read loader from disk
    mov eax, LOADER_START_SECTOR        ; LBA28 address needs 4 bytes to store
    mov bx, LOADER_BASE_ADDR            ; loader will be load here
    mov cx, 1                           ; read 1 sector
    call read_disk_16_bit


    ; print information before jump
    mov dh, 10
    mov dl, 25
    mov ax, jump_prompt
    mov bp, ax
    mov cx, jump_prompt_end - jump_prompt
    mov ax, 0x1301
    mov bx, 0x0c
    int 0x10

    ; assign cpu to loader, mbr is done
    jmp LOADER_BASE_ADDR


; functions

;-------------------------------------------------------------------------------
;description: read n blocks of disk into specified memory using IO (in/out)
read_disk_16_bit:	   
; eax = LBA address of disk block
; ebx = memory address (first byte) of read block in memory
; ecx = number of sector to read
;-------------------------------------------------------------------------------
    ; save register
    mov esi, eax        ; save eax using esi
    mov di, cx          ; save cx using di, di holds number of blocks to read

    ; set sector to read
    mov dx, 0x1f2
    mov al, cl
    out dx, al

    ; recover ax
    mov esi, eax

    ; write LBA address to hard disk routine (0x1f3 ~ 0x1f6)
    mov dx, 0x1f3       ; 0x1f3 = LBA 7~0 digits
    out dx, al

    ; shift eax 8 digits right to get 15~8 digits of LBA address
    mov cl, 8
    shr eax, cl
    mov dx, 0x1f4
    out dx, al

    ; shift eax 8 digits right to get 23~16 digits of LBA address
    shr eax, cl
    mov dx, 0x1f5
    out dx, al

    ; set disk read property and write 27~24 digits of LBA address
    shr eax, cl
    and al, 0b0000_1111     ; set property bits to zero, prepare to set
    or al, 0b1110_0000      ; set property bits to 1110, means using lba addressing mode
    mov dx, 0x1f6
    out dx, al

    ; let the disk routine read disk and put data into buffer
    mov dx, 0x1f7
    mov al, 0x20            ; let disk read
    out dx, al

    ; wait until disk is prepared to send data
    .not_ready:
        nop                 ; do nothing, just wait
        in al, dx           ; read disk status and test read bit
        and al, 0b1000_1000 ; keep fourth and eighth digits
        cmp al, 0b0000_1000 ; eighth digit means the disk is busy reading data, 
                            ; and fourth digit means disk is prepared to transmit data
        jnz .not_ready      ; if disk is not prepared to transmit data, keep waiting

    ; read data
    mov ax, di              ; di holds number of blocks to read.
    mov dx, 256
    mul dx                  ; a block/sector has 512 bytes data, and each time we read a word (2 byte)
                            ; so we need to read (block_num * 512 / 2) = (block_num * 256) times
    mov cx, ax
    mov dx, 0x1f0
    .keep_reading:
        in ax, dx
        mov [bx], ax        ; bx is memory address where we should read data to
        add bx, 2           ; shift a word
        loop .keep_reading


    ret                     ; return after function finish


; Data Section
greeting db "Hello World, JackOS!"
greeting_end:
read_disk_prompt db "Reading loader!"
read_disk_prompt_end:
jump_prompt db "Jumping to loader!"
jump_prompt_end:


; fill start sector (510 bytes + bootable flag)
times 510- ($ - $$) db 7
    db 0x55, 0xaa
