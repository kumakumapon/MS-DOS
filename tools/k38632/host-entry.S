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
    mov WORD PTR cs:host_api_kind,1
    jmp host_dispatch_api
host_raw_dispatch:
    mov WORD PTR cs:host_api_kind,0
    jmp host_dispatch_api
host_chain:
    .byte 0x2e,0xff,0x2e
    .word host_old_int
host_dispatch_api:
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
    mov ss,WORD PTR host_caller_ss
    mov esp,DWORD PTR host_caller_esp
    popw gs
    popw fs
    popw es
    popw ds
    popad
    iretw
host_acquire:
    push ebx
    push esi
    push edi
    push ebp
    mov ah,8
    callw xcall
    mov WORD PTR results+52,ax
    mov WORD PTR results+48,dx
    mov ah,7
    callw xcall
    mov WORD PTR results+500,ax
    mov dx,4100
    mov ah,9
    inc DWORD PTR results+160
    callw xcall
    cmp ax,1
    jne host_no_memory
    mov WORD PTR results+40,dx
    mov ah,12
    callw xcall
    cmp ax,1
    jne host_allocation_error
    mov WORD PTR results+42,1
    movzx eax,dx
    shl eax,16
    mov ax,bx
    mov DWORD PTR results+32,eax
    add eax,4095
    jc host_allocation_error
    and eax,0xfffff000
    cmp eax,0x100000
    jb host_allocation_error
    cmp eax,0xc00000
    ja host_allocation_error
    mov DWORD PTR results+28,eax
    call loader_prepare
    test eax,eax
    jnz host_allocation_error
    xor eax,eax
    jmp host_acquire_return
host_no_memory:
    mov DWORD PTR results+16,8
    jmp host_acquire_error
host_allocation_error:
    mov DWORD PTR results+16,9
host_acquire_error:
    mov DWORD PTR results+12,6
    mov eax,1
host_acquire_return:
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
host_enter:
    push ebx
    push esi
    push edi
    push ebp
    in al,2
    mov BYTE PTR results+140,al
    mov ah,7
    callw xcall
    mov WORD PTR results+500,ax
    mov ah,5
    callw xcall
    cmp ax,1
    jne host_a20_error
    mov WORD PTR results+142,1
    callw run_pm
    callw verify_host
    mov ah,6
    callw xcall
    cmp ax,1
    jne host_a20_error
    mov WORD PTR results+142,0
    mov ah,7
    callw xcall
    mov WORD PTR results+502,ax
    cmp ax,WORD PTR results+500
    je host_enter_return
host_a20_error:
    mov DWORD PTR results+16,10
host_enter_return:
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
host_drop_xms:
    push ebx
    push esi
    push edi
    push ebp
    cmp WORD PTR results+142,0
    je host_drop_lock
    mov ah,6
    callw xcall
    cmp ax,1
    jne host_drop_error
    mov WORD PTR results+142,0
host_drop_lock:
    cmp WORD PTR results+42,0
    je host_drop_handle
    mov dx,WORD PTR results+40
    mov ah,13
    callw xcall
    cmp ax,1
    jne host_drop_error
    mov WORD PTR results+42,0
host_drop_handle:
    cmp WORD PTR results+40,0
    je host_dropped
    mov dx,WORD PTR results+40
    mov ah,10
    callw xcall
    cmp ax,1
    jne host_drop_error
    mov WORD PTR results+40,0
    inc DWORD PTR results+156
host_dropped:
    mov ah,7
    callw xcall
    mov WORD PTR results+502,ax
    mov ah,8
    callw xcall
    mov WORD PTR results+54,ax
    mov WORD PTR results+50,dx
    jmp host_drop_return
host_drop_error:
    mov DWORD PTR results+16,10
host_drop_return:
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
.section .data
host_api_kind: .word 0
host_old_int: .long 0
host_caller_ss: .word 0
host_caller_esp: .long 0
host_ready_text: .ascii "K32HOST READY: two PM32 task slots\r\n$"
host_fail_text: .ascii "K32HOST unavailable: real mode / XMS required\r\n$"
.section .text
.code16
