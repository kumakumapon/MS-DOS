.intel_syntax noprefix
.code16
.global _start
_start:
    mov dx,OFFSET title
    mov ah,9
    int 0x21
again:
    mov ah,1
    int 0x21
    cmp al,27
    jne again
    mov ax,0x4c00
    int 0x21
title: .ascii "V86 ECHO: type keys, ESC exits\r\n$"
