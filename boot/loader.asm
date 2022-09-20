%include "boot.inc"

section loader vstart=LOADER_BASE_ADDR

    ; print loader information
    mov dh, 11
    mov dl, 25
    mov ax, loader_start_prompt
    mov bp, ax
    mov cx, loader_start_prompt_end - loader_start_prompt
    mov ax, 0x1301
    mov bx, 0x0c
    int 0x10

end: jmp end

; Data Sector

loader_start_prompt db "Loader start now!"
loader_start_prompt_end: