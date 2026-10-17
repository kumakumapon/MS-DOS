.intel_syntax noprefix
.code16
.global _start
_start:
    mov ax,0x3000
    mov es,ax
    mov WORD PTR es:[0],0x55aa
    mov ax,0x4c00
    int 0x21
