; Master Boot Record (MBR) program

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
    mov ax, message     ; set bp
    mov bp, ax
    mov cx, message_end - message
    mov ax, 0x1301      ; set AH and AL
    mov bx, 0x0C        ; set BH = 0 and BL = C (Light RED) https://en.wikipedia.org/wiki/BIOS_color_attributes
    int 0x10

    end: jmp end


; Data Section
message db "Hello World, JackOS!"
message_end:


; fill start sector (510 bytes + bootable flag)
times 510- ($ - $$) db 7
    db 0x55, 0xaa
