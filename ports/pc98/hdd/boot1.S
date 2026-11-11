/* PC-98 SASI unit-0 IPL. BIOS enters at 1fc0:0000, with device at 0000:0584. */
.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
 jmp entry
 .byte 0x90,0x90
 .ascii "IPL1"
entry:
 cli
 cld
 mov ax,cs
 mov ds,ax
 mov ax,0x1f00
 mov ss,ax
 mov sp,0x1000
 sti
 xor ax,ax
 mov es,ax
 cmp BYTE PTR es:[0x584],0x80
 jne failed
 mov ax,0x1000
 mov es,ax
 xor bp,bp
 mov ax,0x0600
 mov cx,4
 xor dx,dx
 mov bx,4096
 int 0x1b
 jc failed
 cmp WORD PTR es:[3],0x484b
 jne failed
 cmp WORD PTR es:[5],0x5344
 jne failed
 cmp WORD PTR es:[7],0x5632
 jne failed
 cmp WORD PTR es:[9],0x2131
 jne failed
 ljmp 0x1000,0
failed:
 mov ax,0xa000
 mov es,ax
 mov WORD PTR es:[0],'H'
 mov WORD PTR es:[2],'1'
 cli
1: hlt
 jmp 1b
.org 254
.word 0xaa55
