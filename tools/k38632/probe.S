/* Original PC-98 Ring-3 bring-up. No DOS/BIOS or disk calls in protected mode. */
.intel_syntax noprefix
.code16
.section .text
.global _start, results, user_code, user_end, irq_or_df, pages
.equ UC,0x00800000
.equ UD,0x00801000
.equ US,0x00803000
.equ USER_CS,59
.equ USER_DS,67
.equ HEADER,96
.equ STRIDE,96
.equ CASES,5
_start:
    cld
    push cs
    pop ds
    push cs
    pop es
    smsw ax
    test al,1
    jnz refuse_pe
    in al,2
    test al,1
    jz refuse_irq
    mov BYTE PTR results+76,al
    xor eax,eax
    in al,0x5f
    shl eax,8
    in al,0x5d
    shl eax,8
    in al,0x5c
    mov DWORD PTR results+88,eax
    /* Keep DOS's supplied stack until the 64 KiB allocation is confirmed. */
    mov bx,0x1000
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
    mov DWORD PTR current,0
next_case:
    call run_pm
    mov eax,DWORD PTR current
    imul eax,STRIDE
    add eax,OFFSET results+HEADER
    mov edx,cr0
    mov DWORD PTR [eax+72],edx
    mov edx,cr3
    mov DWORD PTR [eax+76],edx
    mov edx,DWORD PTR results+72
    mov DWORD PTR [eax+80],edx
    in al,2
    movzx edx,al
    mov eax,DWORD PTR current
    imul eax,STRIDE
    add eax,OFFSET results+HEADER
    mov DWORD PTR [eax+84],edx
    mov edx,DWORD PTR results+68
    mov DWORD PTR [eax+92],edx
    call verify_case
    inc DWORD PTR current
    cmp DWORD PTR current,CASES
    jb next_case
    mov eax,cr0
    mov DWORD PTR results+32,eax
    mov eax,cr3
    mov DWORD PTR results+40,eax
    data32 sgdt results+50
    data32 sidt results+62
    in al,2
    mov BYTE PTR results+77,al
    mov si,OFFSET results+44
    mov di,OFFSET results+50
    mov cx,6
    repe cmpsb
    jne bad_host
    mov si,OFFSET results+56
    mov di,OFFSET results+62
    mov cx,6
    repe cmpsb
    je host_checked
bad_host:
    mov DWORD PTR results+16,5
host_checked:
    mov DWORD PTR results+12,0x1f
    cmp DWORD PTR results+16,0
    jne failed
    mov dx,OFFSET passed
    mov ah,9
    int 0x21
    mov ax,0x4c00
    int 0x21
refuse_pe:
    mov WORD PTR results+78,1
    mov dx,OFFSET busy_pe
    jmp refuse
refuse_irq:
    mov WORD PTR results+78,2
    mov dx,OFFSET busy_irq
refuse:
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
failed:
    mov dx,OFFSET failure
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
/* Validate CPU-derived records before printing PASS, also without a browser. */
verify_case:
    mov esi,eax
    mov edx,DWORD PTR [esi+4]
    and edx,0x80000001
    cmp edx,0x80000001
    jne bad_case
    cmp DWORD PTR [esi+20],USER_CS
    jne bad_case
    cmp DWORD PTR [esi+32],USER_DS
    jne bad_case
    mov edx,DWORD PTR [esi+24]
    and edx,0x27200 /* VM, NT, IOPL must be clear; IF must be set. */
    cmp edx,0x200
    jne bad_case
    cmp DWORD PTR [esi+40],USER_CS
    jne bad_case
    cmp DWORD PTR [esi+44],USER_DS
    jne bad_case
    cmp DWORD PTR [esi+64],USER_DS
    jne bad_case
    mov edx,DWORD PTR [esi+48]
    and edx,0x27200
    cmp edx,0x200
    jne bad_case
    mov edx,DWORD PTR results+28
    cmp DWORD PTR [esi+72],edx
    jne bad_case
    mov edx,DWORD PTR results+36
    cmp DWORD PTR [esi+76],edx
    jne bad_case
    mov edx,DWORD PTR [esi+92]
    cmp DWORD PTR [esi+80],edx
    jne bad_case
    movzx edx,BYTE PTR results+76
    cmp DWORD PTR [esi+84],edx
    jne bad_case
    cmp DWORD PTR current,1
    jne not_timer
    cmp DWORD PTR [esi+56],0
    je bad_case
    test DWORD PTR [esi+60],1
    jz bad_case
    ret
not_timer:
    cmp DWORD PTR [esi+56],0x32320001
    jne bad_case
    cmp DWORD PTR current,2
    jne not_pf
    cmp DWORD PTR [esi+36],4
    jne bad_case
    cmp DWORD PTR [esi+12],0xc00000
    jne bad_case
not_pf:
    ret
bad_case:
    mov DWORD PTR results+16,4
    ret
run_pm:
    pushad
    pushfd
    pop DWORD PTR results+68
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
    data32 sgdt results+44
    data32 sidt results+56
    mov eax,cr0
    mov DWORD PTR results+28,eax
    mov eax,cr3
    mov DWORD PTR results+36,eax
    data32 lgdt gdtr
    data32 lidt idtr
    mov eax,cr0
    or eax,1
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
    mov esp,0xffe0
    mov edi,DWORD PTR results+20
    add edi,OFFSET pages+4095
    and edi,0xfffff000
    mov DWORD PTR results+24,edi
    mov edx,edi
    xor eax,eax
    mov ecx,6144
    rep stosd
    mov edi,edx
    lea eax,[edi+4096+3]
    mov DWORD PTR es:[edi],eax /* Low 4 MiB supervisor only. */
    lea eax,[edi+8192+7]
    mov DWORD PTR es:[edi+8],eax /* User linear 8 MiB. */
    xor ecx,ecx
identity:
    mov eax,ecx
    shl eax,12
    or eax,3
    mov DWORD PTR es:[edi+4096+ecx*4],eax
    inc ecx
    cmp ecx,1024
    jb identity
    lea eax,[edi+12288+5]
    mov DWORD PTR es:[edi+8192],eax /* User code read-only. */
    lea eax,[edi+16384+7]
    mov DWORD PTR es:[edi+8192+4],eax
    lea eax,[edi+20480+7]
    mov DWORD PTR es:[edi+8192+12],eax /* Page 2 is a guard. */
    add edi,12288
    mov esi,OFFSET user_code
    mov ecx,user_end-user_code
    rep movsb
    mov edi,DWORD PTR results+24
    mov eax,DWORD PTR current
    mov DWORD PTR es:[edi+16384],eax
    mov DWORD PTR tss+4,0xffe0
    mov WORD PTR tss+8,16
    mov WORD PTR tss+102,104 /* No I/O bitmap; CPL3 port I/O denied. */
    mov BYTE PTR gdt+53,0x89
    mov ax,48
    ltr ax
    mov BYTE PTR idt+14*8+5,0x8e
    cmp DWORD PTR current,4
    jne gate_ready
    mov BYTE PTR idt+14*8+5,0x0e /* #PF delivery -> #NP -> genuine #DF. */
gate_ready:
    mov eax,DWORD PTR results+24
    mov cr3,eax
    mov eax,cr0
    or eax,0x80000000
    mov cr0,eax
    mov al,0xff
    cmp DWORD PTR current,1
    jne mask_ready
    mov al,0xfe
mask_ready:
    out 2,al
    cmp DWORD PTR current,1
    jne enter_user
    mov al,0x30
    out 0x77,al
    xor al,al
    out 0x71,al
    mov al,0x10
    out 0x71,al
enter_user:
    push USER_DS
    push US+4096
    push 0x202 /* CPL3 IF=1, VM=NT=IOPL=0. */
    push USER_CS
    push UC
    iretd

/* IRQ0 is vector 8 on PC-98. IRQ pushes EIP/CS/EFLAGS; #DF first pushes
 * error=0. For our GDT every valid CS < 0x200, so the word at esp+8 cannot
 * have IF set in a #DF frame. IRQ0 can only preempt CPL3 (all ring-0 gates
 * clear IF). Verify the exact CS as well; VM is deliberately clear in both. */
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
    mov ax,ds
    movzx eax,ax
    mov DWORD PTR ss:trap_ds,eax
    mov ax,16
    mov ds,ax
    mov ax,24
    mov es,ax
    mov ebp,esp
    mov edi,DWORD PTR current
    imul edi,STRIDE
    add edi,OFFSET results+HEADER
    mov eax,DWORD PTR [ebp+32]
    mov DWORD PTR [edi],eax
    mov eax,cr0
    mov DWORD PTR [edi+4],eax
    mov eax,cr3
    mov DWORD PTR [edi+8],eax
    mov eax,cr2
    mov DWORD PTR [edi+12],eax
    mov eax,DWORD PTR [ebp+40]
    mov DWORD PTR [edi+16],eax
    mov eax,DWORD PTR [ebp+44]
    mov DWORD PTR [edi+20],eax
    cmp eax,USER_CS
    je valid_caller
    mov DWORD PTR results+16,1
valid_caller:
    mov eax,DWORD PTR [ebp+48]
    mov DWORD PTR [edi+24],eax
    mov eax,DWORD PTR [ebp+52]
    mov DWORD PTR [edi+28],eax
    mov eax,DWORD PTR [ebp+56]
    mov DWORD PTR [edi+32],eax
    mov eax,DWORD PTR [ebp+36]
    mov DWORD PTR [edi+36],eax
    mov eax,DWORD PTR es:[UD+4]
    mov DWORD PTR [edi+40],eax
    mov eax,DWORD PTR es:[UD+8]
    mov DWORD PTR [edi+44],eax
    mov eax,DWORD PTR es:[UD+12]
    mov DWORD PTR [edi+48],eax
    mov eax,DWORD PTR es:[UD+16]
    mov DWORD PTR [edi+52],eax
    mov eax,DWORD PTR es:[UD+20]
    mov DWORD PTR [edi+56],eax
    mov eax,DWORD PTR trap_ds
    mov DWORD PTR [edi+64],eax
    mov eax,DWORD PTR current
    mov DWORD PTR [edi+68],eax
    movzx eax,BYTE PTR idt+14*8+5
    mov DWORD PTR [edi+88],eax
    /* Corroborate hardware IRQ0 through the actual PIC in-service bit. */
    mov al,0x0b
    out 0,al
    in al,0
    movzx edx,al
    mov DWORD PTR [edi+60],edx
    mov al,0x0a
    out 0,al
    mov esi,DWORD PTR current
    mov eax,DWORD PTR expected[esi*4]
    cmp DWORD PTR [edi],eax
    je expected_trap
    mov DWORD PTR results+16,2
expected_trap:
    cmp DWORD PTR [edi],32
    jne leave_pm
    test edx,1
    jnz leave_pm
    mov DWORD PTR results+16,3
leave_pm:
    cli
    mov esp,0xffe0
    mov al,BYTE PTR results+76
    out 2,al
    mov al,0x60
    out 0,al
    mov eax,cr0
    and eax,0x7fffffff
    mov cr0,eax
    mov eax,DWORD PTR results+36
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
    mov eax,DWORD PTR results+28
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
    data32 lidt results+56
    data32 lgdt results+44
    push DWORD PTR results+68
    popfd
    pushfd
    pop DWORD PTR results+72
    popad
    ret
.code32
user_code:
    mov ax,USER_DS
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax
    xor eax,eax
    mov ax,cs
    mov DWORD PTR [UD+4],eax
    mov ax,ss
    mov DWORD PTR [UD+8],eax
    pushfd
    pop eax
    mov DWORD PTR [UD+12],eax
    mov DWORD PTR [UD+16],esp
    mov DWORD PTR [UD+20],0x32320001
    mov eax,DWORD PTR [UD]
    cmp eax,1
    je user_loop
    cmp eax,3
    je user_gp
    cmp eax,2
    je user_pf
    cmp eax,4
    je user_pf
    int 0x30
user_loop:
    mov DWORD PTR [UD+20],0
user_spin:
    inc DWORD PTR [UD+20]
    jmp user_spin
user_gp:
    cli
user_pf:
    mov eax,DWORD PTR [0x00c00000]
    ud2
user_end:
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
current: .long 0
trap_ds: .long 0
expected: .long 48,32,14,13,8
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
results: .ascii "K32CPU01"
    .word 0x100,HEADER+CASES*STRIDE
    .space 68,0
    .long CASES,results,0,0
    .space CASES*STRIDE,0
passed: .ascii "K32CPU PASS: Ring3 PM32, paging, IRQ0, PF, GP, DF, DOS return\r\n$"
failure: .ascii "K32CPU FAIL\r\n$"
busy_pe: .ascii "K32CPU REFUSED: protected/V86 owner present\r\n$"
busy_irq: .ascii "K32CPU REFUSED: IRQ0 must be masked and unowned\r\n$"
.section .bss
.align 16
tss: .space 104
pages: .space 28672
.section .note.GNU-stack,"",@progbits
