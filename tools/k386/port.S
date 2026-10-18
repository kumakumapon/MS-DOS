.intel_syntax noprefix
.code16
.global _start
_start:
    xor al,al
    out 0,al
    mov ax,0x4c00
    int 0x21
