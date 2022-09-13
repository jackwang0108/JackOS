; 设置段寄存器，起点为显存
; set data segment register to monitor memory
mov ax ,0b800h
mov ds, ax

mov byte [0x00], 'H'
mov byte [0x02], 'e'
mov byte [0x04], 'l'
mov byte [0x06], 'l'
mov byte [0x08], '0'
mov byte [0x0a], ' '
mov byte [0x0c], 'W'
mov byte [0x0e], 'o'
mov byte [0x10], 'r'
mov byte [0x12], 'l'
mov byte [0x14], 'd'
mov byte [0x16], ','
mov byte [0x18], ' '
mov byte [0x1a], 'J'
mov byte [0x1c], 'a'
mov byte [0x1e], 'c'
mov byte [0x20], 'k'
mov byte [0x22], 'O'
mov byte [0x24], 'S'

jmp $

times 510-($-$$) db 0
db 0x55,0xaa