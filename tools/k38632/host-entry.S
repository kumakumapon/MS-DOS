/* Included by runtime.S only for the resident bounded-slice backend. */
.code16gcc
.global host_acquire,host_enter,host_drop_xms,initialized,host_api_kind
_start:
    cld
    cli
    mov ax,cs
    mov ds,ax
    mov ss,ax
    mov esp,0xfff0
    sti
    mov bx,0x1010
    mov ax,0x4a00
    int 0x21
    jc host_install_failed
    mov ax,ds
    mov es,ax
    xor eax,eax
    mov edi,OFFSET __bss_start
    mov ecx,OFFSET __bss_end
    sub ecx,edi
    rep stosb
    mov ax,0xd390
    xor bx,bx
    int 0x2f
    cmp ax,0x390d
    je host_already
    call host_install
    test eax,eax
    jnz host_install_failed
    mov ax,0x352f
    int 0x21
    mov WORD PTR host_old_int,bx
    mov WORD PTR host_old_int+2,es
    mov dx,OFFSET host_api
    mov ax,0x252f
    int 0x21
    mov dx,OFFSET host_ready_text
    mov ah,9
    int 0x21
    mov dx,0x1010
    mov ax,0x3100
    int 0x21
host_already:
    mov ax,0x4c00
    int 0x21
host_install_failed:
    mov dx,OFFSET host_fail_text
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
host_api:
    cmp ax,0xd393
    je host_occupancy_query
    cmp ax,0xd391
    je host_manager_api
    cmp ax,0xd390
    jne host_chain
    test bx,bx
    jnz host_raw_dispatch
    mov ax,cs
    mov es,ax
    mov di,OFFSET host_mailbox
    mov ax,0x390d
    iretw
host_manager_api:
    test bx,bx
    jnz host_manager_dispatch
    mov ax,cs
    mov es,ax
    mov di,OFFSET k32_manager
    mov ax,0x391d
    iretw
host_occupancy_query:
    push ds
    push es
    push ecx
    push edx
    push esi
    push edi
    push ebp
    mov ax,cs
    mov ds,ax
    call host_occupied
    mov bx,ax
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop es
    pop ds
    mov ax,0x393d
    iretw
host_manager_dispatch:
    cmp WORD PTR cs:host_entry_busy,0
    jne host_manager_busy
    mov WORD PTR cs:host_api_kind,1
    jmp host_dispatch_api
host_raw_dispatch:
    cmp WORD PTR cs:host_entry_busy,0
    jne host_raw_busy
    mov WORD PTR cs:host_api_kind,0
    jmp host_dispatch_api
    jmp host_dispatch_api
host_manager_busy:
    mov WORD PTR cs:k32_manager+14,5
    iretw
host_raw_busy:
    mov WORD PTR cs:host_mailbox+14,5
    iretw
host_chain:
    .byte 0x2e,0xff,0x2e
    .word host_old_int
host_dispatch_api:
    mov WORD PTR cs:host_entry_busy,1
    cld
    pushad
    pushw ds
    pushw es
    pushw fs
    pushw gs
    mov WORD PTR cs:host_caller_ss,ss
    mov DWORD PTR cs:host_caller_esp,esp
    mov ax,cs
    mov ss,ax
    mov ds,ax
    mov es,ax
    mov esp,0xfff0
    sti
    call host_dispatch
    cli
    mov WORD PTR cs:host_entry_busy,0
    mov ss,WORD PTR host_caller_ss
    mov esp,DWORD PTR host_caller_esp
    popw gs
    popw fs
    popw es
    popw ds
    popad
    iretw
.include "host-engine.S"
.section .data
host_entry_busy: .word 0
host_api_kind: .word 0
host_old_int: .long 0
host_caller_ss: .word 0
host_caller_esp: .long 0
host_ready_text: .ascii "K32HOST READY: two PM32 task slots\r\n$"
host_fail_text: .ascii "K32HOST unavailable: real mode / XMS required\r\n$"
.section .text
.code16
