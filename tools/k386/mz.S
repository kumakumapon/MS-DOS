.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
    .ascii "MZ"
    .word (file_end-_start)%512,(file_end-_start+511)/512,1,4,16,0xffff
    .word 0,0xfef0,0,entry-module,0,28,0
    .word relocated-module,0
    .org 64
module:
entry:
    xor cx,cx
    mov cl,BYTE PTR [0x80]
    push ds
    .byte 0xb8
relocated:
    .word 0
    mov ds,ax
    cmp ax,0x8010
    jne fail
    push 0x5aa5
    pop ax
    cmp ax,0x5aa5
    jne fail
    mov dx,message-module
    mov ah,9
    int 0x21
    pop ds
    mov bx,1
    mov dx,0x81
    mov ah,0x40
    int 0x21
    mov dl,13
    mov ah,2
    int 0x21
    mov dl,10
    int 0x21
    mov ax,0x4c00
    int 0x21
fail:
    mov ax,0x4c01
    int 0x21
message: .ascii "MZ RELOCATION PASS$"
file_end:
