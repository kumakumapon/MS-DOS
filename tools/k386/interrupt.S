.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
    push cs
    pop ds
    push cs
    pop es
    mov ax,ds
    mov WORD PTR block+4,ax
    mov dx,OFFSET child
    mov bx,OFFSET block
    mov ax,0x4b00
    int 0x21
    jc fail
    mov ah,0x4d
    int 0x21
    cmp ax,0x0103
    jne fail
    mov dx,OFFSET passed
    mov ah,9
    int 0x21
    mov ax,0x4c00
    int 0x21
fail:
    mov dx,OFFSET failed
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
child:
.ifdef K386_APP
    .asciz "COUNTER.EXE"
.else
    .asciz "COUNTER.COM"
.endif
tail: .byte 0,13
block: .word 0,tail,0,0,0,0,0
passed: .ascii "INTERRUPT PARENT PASS\r\n$"
failed: .ascii "INTERRUPT PARENT FAIL\r\n$"
