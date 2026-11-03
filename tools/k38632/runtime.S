/* Original K38632 XMS-backed PM32 runner, derived from our Ring-3 probe.
 * Private module: no DOS/BIOS/XMS call while PE is set. */
.intel_syntax noprefix
.code16
.section .text
.global _start, results, user_code, user_end, irq_or_df, pages, context
.equ UC,0x800000
.equ UD,0x801000
.equ GUARD,0xa01000
.equ UM,0xa02000
.ifdef LOADER
.equ US,0xbff000
.else
.equ US,0xa04000
.endif
.global xms_entry
.equ USER_CS,59
.equ USER_DS,67
_start:
    cld
    push cs
    pop ds
    mov ax,cs
    movzx eax,ax
    shl eax,4
    mov DWORD PTR results+20,eax
    mov DWORD PTR results+504,OFFSET results
    smsw ax
    test al,1
    jnz refuse_pe
    in al,2
    test al,1
    jz refuse_irq
    mov BYTE PTR results+140,al
    mov ax,es
    mov WORD PTR results+44,ax
.ifndef LOADER
    xor ecx,ecx
    mov cl,BYTE PTR es:[0x80]
    mov si,0x81
parse:
    jcxz parsed
    mov al,BYTE PTR es:[si]
    inc si
    dec cx
    cmp al,' '
    je parse
    and al,0xdf
    mov BYTE PTR results+46,al
.endif
parsed:
    /* DOS supplied a stack via the MZ header; confirm before private writes. */
    mov bx,0x1010
    mov ah,0x4a
    int 0x21
    jc failed
    mov ax,cs
    mov ss,ax
    mov esp,0xfff0
    mov es,ax
    xor ax,ax
    mov di,OFFSET tss
    mov cx,OFFSET __bss_end
    sub cx,di
    rep stosb
    xor eax,eax
    in al,0x5f
    shl eax,8
    in al,0x5d
    shl eax,8
    in al,0x5c
    mov DWORD PTR results+56,eax
    mov eax,65536
    cmp BYTE PTR results+46,'T'
    jne budget_ready
    mov eax,64
budget_ready:
    mov DWORD PTR results+492,eax
.ifdef LOADER
    data32 call loader_open
    test eax,eax
    jnz loader_rejected
.endif
    mov ax,0x4300
    int 0x2f
    cmp al,0x80
    jne no_xms
    push es
    mov ax,0x4310
    int 0x2f
    mov WORD PTR xms_entry,bx
    mov WORD PTR xms_entry+2,es
    pop es
    mov ah,8
    call xcall
    mov WORD PTR results+52,ax
    mov WORD PTR results+48,dx
    mov ah,7
    call xcall
    mov WORD PTR results+500,ax
    mov dx,4100 /* 4 MiB physical budget plus one alignment page. */
    mov ah,9
    inc DWORD PTR results+160
    call xcall
    cmp ax,1
    jne no_memory
    mov WORD PTR results+40,dx
    mov ah,12
    call xcall
    cmp ax,1
    jne allocation_error
    mov WORD PTR results+42,1
    movzx eax,dx
    shl eax,16
    mov ax,bx
    mov DWORD PTR results+32,eax
    add eax,4095
    jc allocation_error
    and eax,0xfffff000
    cmp eax,0x100000
    jb allocation_error
    cmp eax,0x1000000-0x400000
    ja allocation_error
    mov DWORD PTR results+28,eax
.ifdef LOADER
    data32 call loader_prepare
    test eax,eax
    jnz allocation_error
.endif
    mov ah,5
    call xcall
    cmp ax,1
    jne allocation_error
    mov WORD PTR results+142,1
    mov dx,OFFSET running
    mov ah,9
    int 0x21
schedule:
    call run_pm
.ifdef LOADER
    inc DWORD PTR results+636
.endif
    call verify_host
    cmp DWORD PTR results+16,0
    jne cleanup
    cmp DWORD PTR results+12,2
    jne cleanup
.ifdef LOADER
    cmp DWORD PTR loader_request,0
    je broker_done
    data32 call loader_write
    mov DWORD PTR context+44,eax
broker_done:
.endif
.ifdef LOADER
    mov eax,DWORD PTR results+636
.else
    mov eax,DWORD PTR results+60
.endif
    cmp eax,DWORD PTR results+492
    jae budget_end
    mov ah,0x0b
    int 0x21
    test al,al
    jz schedule
    mov ah,7
    int 0x21
    cmp al,27
    jne schedule
    mov DWORD PTR results+68,65
    jmp cancelled
budget_end:
    mov DWORD PTR results+68,66
cancelled:
    mov DWORD PTR results+12,5
    jmp cleanup
allocation_error:
    mov DWORD PTR results+16,9
    jmp cleanup
no_memory:
    mov DWORD PTR results+12,6
    mov DWORD PTR results+16,8
cleanup:
.ifdef LOADER
    data32 call loader_close
.endif
    cmp DWORD PTR results+16,0
    jne clear_mapping
    call verify_finish
clear_mapping:
    /* Revoke mappings only with the caller CR3 restored, before releasing XMS. */
    cmp DWORD PTR initialized,0
    je mapping_cleared
    mov edi,DWORD PTR results+24
    sub edi,DWORD PTR results+20
    mov DWORD PTR [edi+8],0
    add edi,8192
    xor eax,eax
    mov ecx,1024
    rep stosd
    mov DWORD PTR results+464,1
mapping_cleared:
    cmp WORD PTR results+142,0
    je no_a20
    mov ah,6
    call xcall
    cmp ax,1
    jne cleanup_error
    mov WORD PTR results+142,0
no_a20:
    cmp WORD PTR results+42,0
    je no_lock
    mov dx,WORD PTR results+40
    mov ah,13
    call xcall
    cmp ax,1
    jne cleanup_error
    mov WORD PTR results+42,0
no_lock:
    cmp WORD PTR results+40,0
    je released
    mov dx,WORD PTR results+40
    mov ah,10
    call xcall
    cmp ax,1
    jne cleanup_error
    mov WORD PTR results+40,0
    inc DWORD PTR results+156
released:
    mov ah,7
    call xcall
    mov WORD PTR results+502,ax
    cmp ax,WORD PTR results+500
    jne cleanup_error
    mov ah,8
    call xcall
    mov WORD PTR results+54,ax
    mov WORD PTR results+50,dx
    cmp dx,WORD PTR results+48
    jne cleanup_error
    cmp ax,WORD PTR results+52
    jne cleanup_error
    cmp DWORD PTR results+16,8
    je memory_refused
    cmp DWORD PTR results+16,0
    jne failed
    mov dx,OFFSET finished
    mov ah,9
    int 0x21
    mov ax,0x4c00
.ifdef LOADER
    mov al,BYTE PTR results+628
    cmp DWORD PTR results+12,3
    je return_status
    mov al,255
return_status:
.endif
    int 0x21
cleanup_error:
    mov DWORD PTR results+16,10
failed:
    mov dx,OFFSET failure
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
memory_refused:
    mov dx,OFFSET memory_text
    jmp refused
no_xms:
.ifdef LOADER
    data32 call loader_close
.endif
    mov dx,OFFSET xms_text
    jmp refused
refuse_pe:
    mov dx,OFFSET busy_pe
    jmp refused
refuse_irq:
    mov dx,OFFSET busy_irq
refused:
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
.ifdef LOADER
loader_rejected:
    mov DWORD PTR results+12,6
    cmp DWORD PTR results+16,0
    jne rejection_recorded
    mov DWORD PTR results+16,20
rejection_recorded:
    mov dx,OFFSET rejected_text
    mov ah,9
    int 0x21
    mov ax,0x4c02
    int 0x21
.endif
/* Validate host restoration on every actual preemption, before any DOS call. */
verify_host:
    pushad
    mov eax,DWORD PTR results+92
    cmp eax,DWORD PTR results+96
    jne bad_host
    mov eax,DWORD PTR results+104
    cmp eax,DWORD PTR results+100
    jne bad_host
    mov eax,DWORD PTR results+112
    cmp eax,DWORD PTR results+108
    jne bad_host
    mov al,BYTE PTR results+141
    cmp al,BYTE PTR results+140
    jne bad_host
    mov si,OFFSET results+116
    mov di,OFFSET results+122
    mov cx,6
    repe cmpsb
    jne bad_host
    mov si,OFFSET results+128
    mov di,OFFSET results+134
    mov cx,6
    repe cmpsb
    jne bad_host
    popad
    ret
bad_host:
    mov DWORD PTR results+16,12
    popad
    ret
verify_finish:
.ifdef LOADER
    ret
.endif
    mov al,BYTE PTR results+46
    cmp al,'B'
    je check_cancel
    cmp al,'T'
    je check_budget
    cmp al,'P'
    je check_gp
    mov ebx,4
    mov edx,GUARD
    cmp al,'F'
    je check_pf
    mov ebx,7
    mov edx,UC
    cmp al,'C'
    je check_pf
    mov ebx,5
    mov edx,DWORD PTR results+20
    cmp al,'K'
    je check_pf
    mov ebx,6
    mov edx,US-4
    cmp al,'S'
    je check_pf
    cmp DWORD PTR results+12,3
    jne bad_finish
    cmp DWORD PTR results+144,3
    jne bad_finish
    cmp DWORD PTR results+148,0xfffc0000
    jne bad_finish
    cmp DWORD PTR results+60,0
    je bad_finish
    mov eax,DWORD PTR results+60
    cmp eax,DWORD PTR results+64
    jne bad_finish
    ret
check_pf:
    cmp DWORD PTR results+68,14
    jne bad_finish
    cmp DWORD PTR results+72,edx
    jne bad_finish
    cmp DWORD PTR results+76,ebx
    jne bad_finish
    jmp check_fault
check_gp:
    cmp DWORD PTR results+68,13
    jne bad_finish
    cmp DWORD PTR results+76,0
    jne bad_finish
check_fault:
    cmp DWORD PTR results+12,4
    jne bad_finish
    ret
check_cancel:
    cmp DWORD PTR results+68,65
    jne bad_finish
    jmp check_killed
check_budget:
    cmp DWORD PTR results+68,66
    jne bad_finish
check_killed:
    cmp DWORD PTR results+12,5
    jne bad_finish
    cmp DWORD PTR results+60,0
    je bad_finish
    ret
bad_finish:
    mov DWORD PTR results+16,13
    ret
xcall:
    push ds
    push es
    push ecx
    push esi
    push edi
    push ebp
    .byte 0xff,0x1e
    .word xms_entry
    pop ebp
    pop edi
    pop esi
    pop ecx
    pop es
    pop ds
    ret
run_pm:
    pushad
    pushfd
    pop DWORD PTR results+108
    cli
    mov WORD PTR real_sp,sp
    mov WORD PTR old_fs,fs
    mov WORD PTR old_gs,gs
    mov ax,cs
    mov WORD PTR real_jump+3,ax
    movzx eax,ax
    shl eax,4
    mov DWORD PTR results+20,eax
    mov edx,eax
    mov WORD PTR gdt+8+2,ax
    mov WORD PTR gdt+16+2,ax
    mov WORD PTR gdt+32+2,ax
    mov WORD PTR gdt+40+2,ax
    shr eax,16
    mov BYTE PTR gdt+8+4,al
    mov BYTE PTR gdt+16+4,al
    mov BYTE PTR gdt+32+4,al
    mov BYTE PTR gdt+40+4,al
    add edx,OFFSET tss
    mov WORD PTR gdt+48+2,dx
    shr edx,16
    mov BYTE PTR gdt+48+4,dl
    mov eax,DWORD PTR results+20
    add eax,OFFSET gdt
    mov DWORD PTR gdtr+2,eax
    mov eax,DWORD PTR results+20
    add eax,OFFSET idt
    mov DWORD PTR idtr+2,eax
    data32 sgdt results+116
    data32 sidt results+128
    mov eax,cr0
    mov DWORD PTR results+96,eax
    mov eax,cr3
    mov DWORD PTR results+100,eax
    data32 lgdt gdtr
    data32 lidt idtr
    mov eax,cr0
    or eax,15 /* Refuse unvirtualized x87 state through #NM. */
    mov cr0,eax
    .byte 0x66,0xea
    .long pm_start
    .word 8
.code32
pm_start:
    push 2
    popfd
    mov ax,16
    mov ds,ax
    mov ss,ax
    mov ax,24
    mov es,ax
    mov esp,0xeff0
    cmp DWORD PTR initialized,0
    jne tables_ready
    mov edi,DWORD PTR results+20
    add edi,OFFSET pages+4095
    and edi,0xfffff000
    mov DWORD PTR results+24,edi
    mov ebx,edi
    xor eax,eax
    mov ecx,3072
    rep stosd
    mov edi,ebx
    lea eax,[edi+4096+3]
    mov DWORD PTR es:[edi],eax
    lea eax,[edi+8192+7]
    mov DWORD PTR es:[edi+8],eax
    xor ecx,ecx
identity:
    mov eax,ecx
    shl eax,12
    or eax,3
    mov DWORD PTR es:[edi+4096+ecx*4],eax
    inc ecx
    cmp ecx,1024
    jb identity
.ifdef LOADER
    xor ecx,ecx
map_external:
    mov eax,DWORD PTR loader_map[ecx*4]
    test eax,eax
    jz external_pte
    add eax,DWORD PTR results+28
external_pte:
    mov DWORD PTR es:[edi+8192+ecx*4],eax
    inc ecx
    cmp ecx,1024
    jb map_external
.else
    mov eax,DWORD PTR results+28
    or eax,5
    mov DWORD PTR es:[edi+8192],eax
    mov ecx,1
map_data:
    mov eax,ecx
    shl eax,12
    add eax,DWORD PTR results+28
    or eax,7
    mov DWORD PTR es:[edi+8192+ecx*4],eax
    inc ecx
    cmp ecx,513
    jb map_data
    mov eax,DWORD PTR results+28
    add eax,0x201007
    mov DWORD PTR es:[edi+8192+514*4],eax
    add eax,4096
    mov DWORD PTR es:[edi+8192+516*4],eax
    /* Physical XMS is accessible before paging, with locally enabled A20. */
    mov edi,DWORD PTR results+28
    xor eax,eax
    mov ecx,0x100000
    rep stosd
    mov edi,DWORD PTR results+28
    mov esi,OFFSET user_code
    mov ecx,user_end-user_code
    rep movsb
    mov edi,DWORD PTR results+28
    movzx eax,BYTE PTR results+46
    mov DWORD PTR es:[edi+0x201000],eax
    mov eax,DWORD PTR results+20
    mov DWORD PTR es:[edi+0x201010],eax
.endif
    mov DWORD PTR context,USER_DS
    mov DWORD PTR context+4,USER_DS
    mov DWORD PTR context+8,USER_DS
    mov DWORD PTR context+12,USER_DS
.ifdef LOADER
    mov eax,DWORD PTR loader_entry
    mov DWORD PTR context+56,eax
.else
    mov DWORD PTR context+56,UC
.endif
    mov DWORD PTR context+60,USER_CS
    mov DWORD PTR context+64,0x202
    mov DWORD PTR context+68,US+4096
    mov DWORD PTR context+72,USER_DS
    mov DWORD PTR initialized,1
tables_ready:
    cmp DWORD PTR results+12,2
    jne context_entry
    inc DWORD PTR results+64
context_entry:
    mov DWORD PTR tss+4,0xeff0
    mov WORD PTR tss+8,16
    mov WORD PTR tss+102,104
    mov BYTE PTR gdt+53,0x89
    mov ax,48
    ltr ax
    mov eax,DWORD PTR results+24
    mov cr3,eax
    mov eax,cr0
    or eax,0x80000000
    mov cr0,eax
    mov al,0xfe
    out 2,al
    mov al,0x30
    out 0x77,al
    xor al,al
    out 0x71,al
    mov al,0x10
    out 0x71,al
    mov DWORD PTR results+12,1
    /* Restore every GPR/segment and the real architectural Ring-3 frame. */
    mov ax,16
    mov es,ax
    mov esi,OFFSET context
    mov edi,0xef00
    mov ecx,19
    rep movsd
    mov esp,0xef00
    pop gs
    pop fs
    pop es
    pop ds
    popad
    add esp,8
    iretd
irq_or_df:
    test DWORD PTR [esp+8],0x200
    jz double_fault
    cmp DWORD PTR [esp+4],USER_CS
    jne double_fault
    push 0
    push 32
    jmp exception
double_fault:
    push 8
    jmp exception
.macro GATE n, error=0
vector_\n:
.if !\error
    push 0
.endif
    push \n
    jmp exception
.endm
GATE 0
GATE 1
GATE 2
GATE 3
GATE 4
GATE 5
GATE 6
GATE 7
GATE 9
GATE 10,1
GATE 11,1
GATE 12,1
GATE 13,1
GATE 14,1
GATE 15
GATE 16
GATE 17,1
GATE 18
GATE 19
GATE 20
GATE 21
GATE 22
GATE 23
GATE 24
GATE 25
GATE 26
GATE 27
GATE 28
GATE 29
GATE 30
GATE 31
user_exit:
    push 0
    push 48
exception:
    cld
    pushad
    xor eax,eax
    mov ax,ds
    push eax
    mov ax,es
    push eax
    mov ax,fs
    push eax
    mov ax,gs
    push eax
    mov ax,16
    mov ds,ax
    mov es,ax
    mov esi,esp
    mov edi,OFFSET context
    mov ecx,19
    rep movsd
    mov esi,OFFSET context
    mov edi,OFFSET results+320
    mov ecx,19
    rep movsd
    mov eax,cr0
    mov DWORD PTR results+84,eax
    mov eax,cr3
    mov DWORD PTR results+88,eax
    mov eax,cr2
    mov DWORD PTR results+72,eax
    mov eax,DWORD PTR context+52
    mov DWORD PTR results+76,eax
    mov eax,DWORD PTR context+48
    mov DWORD PTR results+68,eax
    mov DWORD PTR results+12,4
    cmp DWORD PTR context+60,USER_CS
    jne invalid_context
    cmp DWORD PTR context+72,USER_DS
    jne invalid_context
    mov eax,DWORD PTR context+64
    and eax,0x27200
    cmp eax,0x200
    jne invalid_context
    mov al,0x0b
    out 0,al
    in al,0
    movzx eax,al
    mov DWORD PTR results+80,eax
    mov al,0x0a
    out 0,al
    cmp DWORD PTR context+48,32
    jne not_timer
    test DWORD PTR results+80,1
    jz invalid_context
    cmp DWORD PTR results+60,0
    jne later_irq
    mov esi,OFFSET context
    mov edi,OFFSET results+168
    mov ecx,19
    rep movsd
later_irq:
    inc DWORD PTR results+60
    mov esi,OFFSET context
    mov edi,OFFSET results+244
    mov ecx,19
    rep movsd
    mov DWORD PTR results+12,2
    jmp capture
not_timer:
.ifdef LOADER
    jmp loader_trap
.endif
    cmp DWORD PTR context+48,48
    jne capture
    cmp DWORD PTR context+44,0
    jne invalid_context
    mov DWORD PTR results+12,3
    jmp capture
invalid_context:
    mov DWORD PTR results+16,11
capture:
    mov ax,24
    mov es,ax
.ifdef LOADER
    mov ebx,DWORD PTR loader_rwbase
    cmp DWORD PTR loader_rwbytes,0x200000
    jb external_no_samples
    xor ecx,ecx
external_samples:
    mov esi,DWORD PTR sample_offsets[ecx*4]
    mov eax,DWORD PTR es:[ebx+esi]
    mov DWORD PTR results+396[ecx*4],eax
    inc ecx
    cmp ecx,16
    jb external_samples
    mov DWORD PTR results+152,16
external_no_samples:
    cmp DWORD PTR loader_rwbytes,4
    jb external_no_counter
    mov ebx,DWORD PTR loader_rwbase
    mov eax,DWORD PTR es:[ebx]
    mov DWORD PTR results+468,eax
external_no_counter:
    jmp pte_capture
.endif
    mov eax,DWORD PTR es:[UM+4]
    mov DWORD PTR results+144,eax
    mov eax,DWORD PTR es:[UM+8]
    mov DWORD PTR results+148,eax
    mov eax,DWORD PTR es:[UM+12]
    mov DWORD PTR results+468,eax
    /* Keep actual data across 64 KiB and final 2 MiB boundaries in the record. */
    xor ecx,ecx
sample:
    mov esi,DWORD PTR sample_offsets[ecx*4]
    mov eax,DWORD PTR es:[UD+esi]
    mov DWORD PTR results+396[ecx*4],eax
    inc ecx
    cmp ecx,16
    jb sample
    mov DWORD PTR results+152,16
pte_capture:
    xor ecx,ecx
pte_sample:
    mov esi,DWORD PTR pte_offsets[ecx*4]
    add esi,DWORD PTR results+24
    mov eax,DWORD PTR es:[esi]
    mov DWORD PTR results+512[ecx*4],eax
    inc ecx
    cmp ecx,13
    jb pte_sample
.ifdef LOADER
    jmp leave_pm
loader_trap:
    cmp DWORD PTR context+48,48
    jne capture
    inc DWORD PTR results+608
    mov eax,DWORD PTR context+44
    test eax,eax
    jz loader_exit
    cmp eax,1
    je loader_output
    cmp eax,2
    je loader_yield
    cmp eax,3
    je loader_ticks
    mov DWORD PTR context+44,-38
    inc DWORD PTR results+632
    jmp loader_resume
loader_exit:
    mov eax,DWORD PTR context+32
    mov DWORD PTR results+628,eax
    mov DWORD PTR results+12,3
    jmp capture
loader_yield:
    mov DWORD PTR context+44,0
    inc DWORD PTR results+616
    jmp loader_resume
loader_ticks:
    xor eax,eax
    in al,0x5f
    shl eax,8
    in al,0x5d
    shl eax,8
    in al,0x5c
    mov DWORD PTR context+44,eax
    inc DWORD PTR results+620
    jmp loader_resume
loader_output:
    mov ecx,DWORD PTR context+40
    cmp ecx,4096
    ja bad_length
    test ecx,ecx
    jz empty_write
    mov esi,DWORD PTR context+32
    cmp esi,UC
    jb bad_pointer
    mov edx,esi
    add edx,ecx
    jc bad_pointer
    cmp edx,0xc00000
    ja bad_pointer
    dec edx
    sub edx,UC
    shr edx,12
    mov eax,esi
    sub eax,UC
    shr eax,12
    mov ebx,DWORD PTR results+24
    mov di,24
    mov es,di
check_user_page:
    mov edi,DWORD PTR es:[ebx+8192+eax*4]
    and edi,5
    cmp edi,5
    jne bad_pointer
    cmp eax,edx
    je copy_user_output
    inc eax
    jmp check_user_page
copy_user_output:
    mov DWORD PTR loader_length,ecx
    mov edi,OFFSET loader_buffer
    mov ax,16
    mov es,ax
    mov ax,USER_DS
    mov ds,ax
    rep movsb
    mov ax,16
    mov ds,ax
    mov ax,24
    mov es,ax
    mov DWORD PTR loader_request,1
    inc DWORD PTR results+612
    jmp loader_resume
bad_pointer:
    inc DWORD PTR results+624
    mov DWORD PTR context+44,-14
    jmp loader_resume
bad_length:
    mov DWORD PTR context+44,-22
    jmp loader_resume
empty_write:
    mov DWORD PTR context+44,0
loader_resume:
    mov DWORD PTR results+12,2
    jmp capture
.endif
leave_pm:
    cli
    mov esp,0xeff0
    mov al,BYTE PTR results+140
    out 2,al
    cmp DWORD PTR context+48,32
    jne no_eoi
    mov al,0x60
    out 0,al
no_eoi:
    mov eax,cr0
    and eax,0x7fffffff
    mov cr0,eax
    mov eax,DWORD PTR results+100
    mov cr3,eax
    .byte 0xea
    .long pm16
    .word 32
.code16
pm16:
    mov ax,40
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov eax,DWORD PTR results+96
    mov cr0,eax
real_jump:
    .byte 0xea
    .word real_return
    .word 0
real_return:
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov ax,WORD PTR old_fs
    mov fs,ax
    mov ax,WORD PTR old_gs
    mov gs,ax
    mov sp,WORD PTR real_sp
    data32 lidt results+128
    data32 lgdt results+116
    push DWORD PTR results+108
    popfd
    pushfd
    pop DWORD PTR results+112
    mov eax,cr0
    mov DWORD PTR results+92,eax
    mov eax,cr3
    mov DWORD PTR results+104,eax
    data32 sgdt results+122
    data32 sidt results+134
    in al,2
    mov BYTE PTR results+141,al
    popad
    ret
.ifndef LOADER
.include "memory-app.S"
.else
user_code:
user_end:
.endif
.section .data
.align 8
gdt:
    .quad 0
    .word 0xffff,0
    .byte 0,0x9a,0x40,0
    .word 0xffff,0
    .byte 0,0x92,0x40,0
    .word 0xffff,0
    .byte 0,0x92,0xcf,0
    .word 0xffff,0
    .byte 0,0x9a,0,0
    .word 0xffff,0
    .byte 0,0x92,0,0
    .word 103,0
    .byte 0,0x89,0,0
    .word 0xffff,0
    .byte 0,0xfa,0xcf,0
    .word 0xffff,0
    .byte 0,0xf2,0xcf,0
gdtr: .word 71
    .long 0
idtr: .word 49*8-1
    .long 0
real_sp: .word 0
old_fs: .word 0
old_gs: .word 0
xms_entry: .long 0
.align 8
idt:
.macro IDT n, label
    .word \label,8
    .byte 0,0x8e
    .word 0
.endm
.irp n,0,1,2,3,4,5,6,7
IDT \n,vector_\n
.endr
IDT 8,irq_or_df
.irp n,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
IDT \n,vector_\n
.endr
.rept 16
    .quad 0
.endr
    .word user_exit,8
    .byte 0,0xee
    .word 0
.align 4
results:
.ifdef LOADER
    .ascii "K32RUN01"
.else
    .ascii "K32MEM01"
.endif
    .word 0x100,1024
    .space 24,0
    .long 0x401000
    .space 432,0
    .long UC,UD,UM,US,0x200000,65536
    .space 528,0
pte_offsets:
    .long 0,8,8192,8196,8192+16*4,8192+17*4,8192+256*4,8192+257*4
    .long 8192+512*4,8192+513*4,8192+514*4,8192+515*4,8192+516*4
sample_offsets:
    .long 0,4,8,12,0xfff8,0xfffc,0x10000,0x10004
    .long 0xffff8,0xffffc,0x100000,0x100004,0x1ffff0,0x1ffff4,0x1ffff8,0x1ffffc
 .ifdef LOADER
rejected_text: .ascii "K32RUN REJECTED: invalid EXE/ELF\r\n$"
running: .ascii "K32RUN RUNNING\r\n$"
finished: .ascii "K32RUN DONE: resources reclaimed\r\n$"
failure: .ascii "K32RUN FAIL\r\n$"
.else
running: .ascii "K32MEM RUNNING: XMS PM32\r\n$"
finished: .ascii "K32MEM DONE: resources reclaimed\r\n$"
failure: .ascii "K32MEM FAIL\r\n$"
.endif
memory_text:
.ifdef LOADER
    .ascii "K32RUN REFUSED: XMS memory\r\n$"
.else
    .ascii "K32MEM REFUSED: XMS memory\r\n$"
.endif
xms_text:
.ifdef LOADER
    .ascii "K32RUN REFUSED: XMS driver required\r\n$"
.else
    .ascii "K32MEM REFUSED: XMS driver required\r\n$"
.endif
busy_pe:
.ifdef LOADER
    .ascii "K32RUN REFUSED: protected/V86 owner present\r\n$"
.else
    .ascii "K32MEM REFUSED: protected/V86 owner present\r\n$"
.endif
busy_irq:
.ifdef LOADER
    .ascii "K32RUN REFUSED: IRQ0 must be masked and unowned\r\n$"
.else
    .ascii "K32MEM REFUSED: IRQ0 must be masked and unowned\r\n$"
.endif
.section .bss
.align 16
tss: .space 104
context: .space 76
initialized: .space 4
pages: .space 16384
.section .note.GNU-stack,"",@progbits
