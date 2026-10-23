.intel_syntax noprefix
.code16
.global _start
_start:
    mov DWORD PTR [0x800],0
    mov dx,OFFSET title
    mov ah,9
    int 0x21
counter:
    inc DWORD PTR [0x800]
    test BYTE PTR [0x800],255
    jnz counter
    mov ah,6
    mov dl,255
    int 0x21
    jz counter
    cmp al,27
    je quit
    cmp al,'q'
    jne counter
quit:
    mov ax,0x4c00
    int 0x21
title: .ascii "V86 COUNTER: independent background CPU loop\r\n$"
