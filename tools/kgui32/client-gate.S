.intel_syntax noprefix
.code16gcc
.section .text
.global kg_gate
kg_gate:
    cmp ax,0xd394
    jne kg_chain
    test bx,bx
    jz kg_query
    pushad
    pushw ds
    pushw es
    mov WORD PTR cs:kg_sender_segment,es
    mov WORD PTR cs:kg_sender_offset,di
    mov WORD PTR cs:kg_caller_ss,ss
    mov DWORD PTR cs:kg_caller_esp,esp
    mov ax,cs
    mov ss,ax
    mov ds,ax
    mov es,ax
    mov esp,0xdff0
    cld
    call kg_dispatch
    mov ss,WORD PTR cs:kg_caller_ss
    mov esp,DWORD PTR cs:kg_caller_esp
    popw es
    popw ds
    popad
kg_query:
    mov ax,0x394d
    iretw
kg_chain:
    .byte 0x2e,0xff,0x2e
    .word kg_old_vector
.section .data
.global kg_old_vector,kg_sender_segment,kg_sender_offset
kg_old_vector: .long 0
kg_sender_segment: .word 0
kg_sender_offset: .word 0
kg_caller_ss: .word 0
kg_caller_esp: .long 0
