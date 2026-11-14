.intel_syntax noprefix
.code16gcc
.section .text
.global _start,io_api
_start:
    cld
    cli
    mov ax,cs
    mov ds,ax
    mov ss,ax
    mov esp,0xfff0
    sti
    mov bx,0x1010
    mov ah,0x4a
    int 0x21
    jc failed
    mov ax,ds
    mov es,ax
    xor eax,eax
    mov edi,OFFSET __bss_start
    mov ecx,OFFSET __bss_end
    sub ecx,edi
    rep stosb
    call io_install
    cmp eax,2
    je complete
    test eax,eax
    jz failed
    mov dx,0x1010
    mov ax,0x3100
    int 0x21
complete:
    mov ax,0x4c00
    int 0x21
failed:
    mov ax,0x4c01
    int 0x21
io_api:
    cmp ax,0xd39f
    jne chain
    test bx,bx
    jz query
    cmp bx,1
    jne reply
    cmp WORD PTR cs:gate_busy,0
    jne reply
    mov WORD PTR cs:gate_busy,1
    pushad
    pushw ds
    pushw es
    mov WORD PTR cs:io_sender_segment,es
    mov WORD PTR cs:io_sender_offset,di
    mov WORD PTR cs:old_ss,ss
    mov DWORD PTR cs:old_sp,esp
    mov ax,cs
    mov ss,ax
    mov ds,ax
    mov es,ax
    mov esp,0xfff0
    cld
    sti
    call io_dispatch
    cli
    mov WORD PTR cs:gate_busy,0
    mov ss,WORD PTR cs:old_ss
    mov esp,DWORD PTR cs:old_sp
    popw es
    popw ds
    popad
reply:
    mov ax,0x39fd
    iretw
query:
    mov ax,cs
    mov es,ax
    mov di,OFFSET io_record
    jmp reply
chain:
    .byte 0x2e,0xff,0x2e
    .word io_old_vector
.section .data
.global io_old_vector,io_sender_segment,io_sender_offset
io_old_vector: .long 0
io_sender_segment: .word 0
io_sender_offset: .word 0
old_ss: .word 0
old_sp: .long 0
gate_busy: .word 0
