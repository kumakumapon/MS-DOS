.intel_syntax noprefix
.code16
.global _start
_start:
    mov dx,OFFSET title
    mov ah,9
    int 0x21
counter:
    inc DWORD PTR [0x800]
    jmp counter
title: .ascii "V86 COUNTER: independent background CPU loop\r\n$"
