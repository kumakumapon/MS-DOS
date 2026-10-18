/* A tampered private IRQ callback must fault only this session. */
.intel_syntax noprefix
.code16
.global _start
_start:
    mov ax,cs
    mov es,ax
    mov bx,OFFSET callback
    mov cx,1
    mov ah,2
    int 0x1c
    xor ax,ax
    mov es,ax
    mov WORD PTR es:[0x1e],0xe000
    mov ah,8
    int 0x21
    jmp _start
callback:
    iret
