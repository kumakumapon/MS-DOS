/* Shared bounded-slice CPU/XMS engine; resident host and independent GUI loader. */
.code16gcc
.global host_acquire,host_enter,host_drop_xms
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
