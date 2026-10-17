.intel_syntax noprefix
.code16
.global _start
_start:
    std
    mov dx,OFFSET text
    mov ah,9
    int 0x21
    cld
    mov ax,0x4c00
    int 0x21
text: .ascii "V86 DIRECTION FLAG PASS\r\n$"
