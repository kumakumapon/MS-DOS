.intel_syntax noprefix
.code16
.global _start
_start:
    mov dx,OFFSET filename
    mov ax,0x3d00
    int 0x21
    jc fail
    mov bx,ax
    mov dx,OFFSET buffer
    mov cx,16
    mov ah,0x3f
    int 0x21
    jc fail
    mov WORD PTR length,ax
    mov cx,1
    mov ah,0x40
    int 0x21
    jnc fail
    cmp ax,5
    jne fail
    mov ah,0x3e
    int 0x21
    jc fail
    mov bx,1
    mov cx,WORD PTR length
    mov dx,OFFSET buffer
    mov ah,0x40
    int 0x21
    jc fail
    mov dx,OFFSET denied
    xor cx,cx
    mov ah,0x3c
    int 0x21
    jnc fail
    cmp ax,5
    jne fail
    mov dx,OFFSET pass
    mov ah,9
    int 0x21
    mov ax,0x4c00
    int 0x21
fail:
    mov dx,OFFSET failure
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
filename: .asciz "KDATA.TXT"
denied: .asciz "DENIED.TXT"
pass: .ascii "READ ONLY FILE PASS\r\n$"
failure: .ascii "FILE CHECK FAIL\r\n$"
length: .word 0
buffer: .space 32,0
