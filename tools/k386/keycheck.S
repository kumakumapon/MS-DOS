.intel_syntax noprefix
.code16
.global _start
_start:
    mov dx,OFFSET waiting
    mov ah,9
    int 0x21
peek:
    mov ah,1
    xor bx,bx
    int 0x18
    test bh,bh
    jz peek
    cmp al,'X'
    jne fail
    mov ah,1
    xor bx,bx
    int 0x18
    test bh,bh
    jz fail
    cmp al,'X'
    jne fail
    xor ax,ax
    int 0x18
    cmp al,'X'
    jne fail
    mov ah,1
    int 0x18
    test bh,bh
    jnz fail
    mov dx,OFFSET success
    mov ah,9
    int 0x21
    mov ax,0x4c00
    int 0x21
fail:
    mov ax,0x4c01
    int 0x21
waiting: .ascii "PC98 KEY PEEK WAIT: type X\r\n$"
success: .ascii "PC98 KEY PEEK PASS\r\n$"
