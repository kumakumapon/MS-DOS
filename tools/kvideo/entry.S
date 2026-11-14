.intel_syntax noprefix
.code16gcc
.section .text
.global _start,kv_api
_start:
    cld
    cli
    mov ax,cs
    mov ds,ax
    mov ss,ax
    mov esp,0x5ff0
    sti
    mov bx,0x610
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
    call kv_install
    cmp eax,2
    je complete
    test eax,eax
    jz failed
    mov dx,0x610
    mov ax,0x3100
    int 0x21
complete:
    mov ax,0x4c00
    int 0x21
failed:
    mov ax,0x4c01
    int 0x21
kv_api:
    cmp ax,0xd39e
    jne chain
    test bx,bx
    jz query
    cmp bx,1
    jne reply
    pushad
    pushw ds
    pushw es
    mov WORD PTR cs:kv_sender_segment,es
    mov WORD PTR cs:kv_sender_offset,di
    mov WORD PTR cs:old_ss,ss
    mov DWORD PTR cs:old_sp,esp
    mov ax,cs
    mov ss,ax
    mov ds,ax
    mov es,ax
    mov esp,0x5ff0
    cld
    call kv_dispatch
    mov ss,WORD PTR cs:old_ss
    mov esp,DWORD PTR cs:old_sp
    popw es
    popw ds
    popad
reply:
    mov ax,0x39ed
    iretw
query:
    mov ax,cs
    mov es,ax
    mov di,OFFSET kv_record
    jmp reply
chain:
    .byte 0x2e,0xff,0x2e
    .word kv_old_vector
.section .data
.global kv_old_vector,kv_sender_segment,kv_sender_offset
kv_old_vector: .long 0
kv_sender_segment: .word 0
kv_sender_offset: .word 0
old_ss: .word 0
old_sp: .long 0
