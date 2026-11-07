.intel_syntax noprefix
.code16gcc
.section .text
.global fault_begin,fault_end
fault_begin:
 pushad
 pushw ds
 pushw es
 mov ax,0x351b
 int 0x21
 mov WORD PTR cs:old,bx
 mov WORD PTR cs:old+2,es
 mov WORD PTR cs:count,0
 pushw cs
 popw ds
 mov dx,OFFSET fail_second
 mov ax,0x251b
 int 0x21
 popw es
 popw ds
 popad
 ret
fault_end:
 pushad
 pushw ds
 mov dx,WORD PTR cs:old
 mov ds,WORD PTR cs:old+2
 mov ax,0x251b
 int 0x21
 popw ds
 popad
 ret
.code16
fail_second:
 cmp ax,0x0600
 jne chain
 inc WORD PTR cs:count
 cmp WORD PTR cs:count,2
 jne chain
 mov ah,0x60
 push bp
 mov bp,sp
 or WORD PTR ss:[bp+6],1
 pop bp
 iretw
chain:
 .byte 0x2e,0xff,0x2e
 .word old
old:.long 0
count:.word 0
.section .note.GNU-stack,"",@progbits
