; PC-98 floppy IPL. Image builder patches contiguous payload counts below.
.8086
BOOT SEGMENT USE16
ORG 0
    jmp SHORT entry
    nop
    db 'MSDOS98 '
    dw 1024
    db 1
    dw 1
    db 2
    dw 192,1232
    db 0feh
    dw 2,8,2
    dd 0
entry:
    cli
    mov ax,cs
    mov ds,ax
    mov ax,1f00h
    mov ss,ax
    mov sp,1000h
    sti
    xor ax,ax
    mov es,ax
    mov al,es:[584h]
    mov drive,al
    mov ax,0c0h
    mov es,ax
    xor bp,bp
    mov si,11 ; fixed data-start LBA
    mov di,WORD PTR bios_sectors
    call load
    mov ax,3000h
    mov es,ax
    xor bp,bp
    mov di,WORD PTR dos_sectors
    call load
    db 0eah
    dw 0,0c0h
load:
    push si
    push di
    push bp
    push es
    mov ax,si
    xor dx,dx
    mov bx,16
    div bx
    mov cl,al
    mov ch,3
    mov ax,dx
    xor dx,dx
    mov bx,8
    div bx
    mov dh,al
    inc dl
    mov al,drive
    mov ah,56h
    mov bx,1024
    int 1bh
    pop es
    pop bp
    pop di
    pop si
    jc failure
    add bp,1024
    inc si
    dec di
    jnz load
    ret
failure:
    mov ax,0a000h
    mov es,ax
    mov WORD PTR es:[0],045h ; E
    cli
hang: hlt
    jmp hang
drive db 90h
    db 'COUNTS'
bios_sectors dw 0
dos_sectors dw 0
    ORG 510
    dw 0aa55h
    ORG 1023
    db 0
BOOT ENDS
END
