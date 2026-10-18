/* Original PC-98 386 bring-up probe. No firmware/disk I/O while protected. */
.intel_syntax noprefix
.code16
.section .text
.global _start, pm_start, vm_code, results
_start:
    cld
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,0xfff0
    mov bx,0x1000
    mov ah,0x4a
    int 0x21
    jc failed
    smsw ax
    test al,1
    jnz busy
    pushfd
    pop DWORD PTR saved_flags
    cli
    mov WORD PTR saved_sp,sp
    mov ax,cs
    mov WORD PTR real_jump+3,ax
    movzx eax,ax
    shl eax,4
    mov DWORD PTR base,eax
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
    mov eax,DWORD PTR base
    add eax,OFFSET gdt
    mov DWORD PTR gdtr+2,eax
    mov eax,DWORD PTR base
    add eax,OFFSET idt
    mov DWORD PTR idtr+2,eax
    sgdt saved_gdtr
    sidt saved_idtr
    mov eax,cr0
    mov DWORD PTR saved_cr0,eax
    mov eax,cr3
    mov DWORD PTR saved_cr3,eax
    xor eax,eax
    mov ax,cs
    mov WORD PTR results+8,ax
    lgdt gdtr
    lidt idtr
    mov eax,cr0
    or eax,1
    mov cr0,eax
    .byte 0x66,0xea
    .long pm_start
    .word 8
busy:
    mov dx,OFFSET busy_text
    jmp print_failure
failed:
    mov dx,OFFSET failed_text
print_failure:
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21

.code32
pm_start:
    push 2
    popfd /* Real-mode BIOS flags may contain NT/IOPL; never inherit them. */
    mov ax,16
    mov ds,ax
    mov ss,ax
    mov ax,24
    mov es,ax
    mov esp,0xffe0
    mov eax,cr0
    mov DWORD PTR results+12,eax
    /* Build the identity map twice; only each private V86 page is user RW. */
    mov edi,DWORD PTR base
    add edi,OFFSET pages+4095
    and edi,0xfffff000
    mov DWORD PTR pd0,edi
    lea eax,[edi+8192]
    mov DWORD PTR pd1,eax
    lea eax,[edi+16384]
    mov DWORD PTR frame0,eax
    lea eax,[edi+20480]
    mov DWORD PTR frame1,eax
    mov edx,edi
    xor eax,eax
    mov ecx,6144
    rep stos DWORD PTR es:[edi]
    mov edi,edx
    lea eax,[edi+4096+7]
    mov DWORD PTR es:[edi],eax
    lea eax,[edi+12288+7]
    mov DWORD PTR es:[edi+8192],eax
    xor ecx,ecx
map:
    mov eax,ecx
    shl eax,12
    or eax,3
    mov DWORD PTR es:[edi+4096+ecx*4],eax
    mov DWORD PTR es:[edi+12288+ecx*4],eax
    inc ecx
    cmp ecx,1024
    jb map
    mov eax,DWORD PTR frame0
    or eax,7
    mov DWORD PTR es:[edi+4096+0x80*4],eax
    mov eax,DWORD PTR frame1
    or eax,7
    mov DWORD PTR es:[edi+12288+0x80*4],eax
    /* Copy a real 8086 program into physically separate pages. */
    mov edi,DWORD PTR frame0
    mov esi,OFFSET vm_code
    mov ecx,vm_end-vm_code
    rep movsb
    mov edi,DWORD PTR frame1
    mov esi,OFFSET vm_code
    mov ecx,vm_end-vm_code
    rep movsb
    mov edi,DWORD PTR frame0
    mov DWORD PTR es:[edi+0x200],0x11110000
    mov edi,DWORD PTR frame1
    mov DWORD PTR es:[edi+0x200],0x22220000
    /* Ring-0 stack and a denied I/O bitmap for the V86 #GP gate. */
    mov DWORD PTR tss+4,0xffe0
    mov WORD PTR tss+8,16
    mov WORD PTR tss+102,104
    mov ax,48
    ltr ax
    mov eax,DWORD PTR pd0
    mov cr3,eax
    mov eax,cr0
    or eax,0x80000000
    mov cr0,eax
    mov DWORD PTR results+16,eax
    ud2
    mov eax,DWORD PTR es:[0x400000] /* not present: supervisor #PF */
    xor ebx,ebx
enter_vm:
    mov DWORD PTR current,ebx
    mov eax,DWORD PTR pd0[ebx*4]
    mov cr3,eax
    mov esp,0xffe0
    push 0
    push 0
    push 0x8000
    push 0x8000
    push 0x8000
    push 0xff0
    push 0x20002
    push 0x8000
    push 0
    iretd
vm_done:
    mov ebx,DWORD PTR current
    mov eax,DWORD PTR es:[0x80200]
    mov DWORD PTR results+32[ebx*4],eax
    mov eax,DWORD PTR es:[0x80204]
    mov DWORD PTR results+40[ebx*4],eax
    cmp ebx,0
    jne leave_pm
    mov ebx,1
    jmp enter_vm
leave_pm:
    mov eax,cr0
    and eax,0x7fffffff
    mov cr0,eax
    mov eax,DWORD PTR saved_cr3
    mov cr3,eax
    /* Switch to a 16-bit descriptor before clearing PE. */
    .byte 0xea
    .long pm16
    .word 32

ud_gate:
    push 0
    push 6
    jmp exception
gp_gate:
    push 13
    jmp exception
pf_gate:
    push 14
exception:
    pushad
    mov ax,16
    mov ds,ax
    mov ax,24
    mov es,ax
    mov ebp,esp
    mov eax,DWORD PTR [ebp+48]
    test eax,0x20000
    jz supervisor
    mov ebx,DWORD PTR current
    mov DWORD PTR results+48[ebx*4],eax
    cmp DWORD PTR [ebp+32],13
    jne unexpected
    mov esi,DWORD PTR [ebp+40]
    add esi,0x80000
    mov al,BYTE PTR es:[esi]
    cmp al,0xcd
    je virtual_int
    cmp al,0xfa
    je virtual_cli
    cmp al,0xf4
    jne unexpected
    inc DWORD PTR results+28
    jmp vm_done
virtual_int:
    cmp BYTE PTR es:[esi+1],0x21
    jne unexpected
    inc DWORD PTR results+24
    mov DWORD PTR [ebp+28],0xabcd
    add DWORD PTR [ebp+40],2
    jmp restore
virtual_cli:
    inc DWORD PTR results+24
    inc DWORD PTR [ebp+40]
    jmp restore
supervisor:
    cmp DWORD PTR [ebp+32],6
    jne page_fault
    inc DWORD PTR results+20
    add DWORD PTR [ebp+40],2
    jmp restore
page_fault:
    cmp DWORD PTR [ebp+32],14
    jne unexpected
    inc DWORD PTR results+60
    mov eax,cr2
    mov DWORD PTR results+64,eax
    add DWORD PTR [ebp+40],6 /* ES + A1 absolute moffs32 */
    jmp restore
unexpected:
    mov DWORD PTR results+68,1
    jmp leave_pm
restore:
    popad
    add esp,8
    iretd

.code16
pm16:
    mov ax,40
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov eax,DWORD PTR saved_cr0
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
    mov sp,WORD PTR saved_sp
    lidt saved_idtr
    lgdt saved_gdtr
    push DWORD PTR saved_flags
    popfd
    mov eax,cr0
    mov DWORD PTR results+56,eax
    cmp DWORD PTR results+68,0
    jne failed
    cmp DWORD PTR results+20,1
    jne failed
    cmp DWORD PTR results+24,4
    jne failed
    cmp DWORD PTR results+28,2
    jne failed
    cmp DWORD PTR results+32,0x11110001
    jne failed
    cmp DWORD PTR results+36,0x22220001
    jne failed
    cmp DWORD PTR results+40,0xabcd
    jne failed
    cmp DWORD PTR results+44,0xabcd
    jne failed
    cmp DWORD PTR results+60,1
    jne failed
    mov dx,OFFSET pass_text
    mov ah,9
    int 0x21
    mov ax,0x4c00
    int 0x21

/* V86 bytes use offsets within their own private page, not monitor labels. */
vm_code:
    inc DWORD PTR [0x200]
    int 0x21
    mov DWORD PTR [0x204],eax
    cli
    hlt
vm_end:

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
gdtr: .word 55
    .long 0
idtr: .word 32*8-1
    .long 0
saved_idtr: .word 0
    .long 0
saved_gdtr: .word 0
    .long 0
saved_cr0: .long 0
saved_cr3: .long 0
saved_flags: .long 0
saved_sp: .word 0
base: .long 0
pd0: .long 0
pd1: .long 0
frame0: .long 0
frame1: .long 0
current: .long 0
.align 8
idt:
.rept 6
    .quad 0
.endr
    .word ud_gate,8
    .byte 0,0x8e
    .word 0
.rept 6
    .quad 0
.endr
    .word gp_gate,8
    .byte 0,0x8e
    .word 0
    .word pf_gate,8
    .byte 0,0x8e
    .word 0
.rept 17
    .quad 0
.endr
results: .ascii "K386CPU1"
    .space 64,0
pass_text: .asciz "K386 PROBE PASS: PM, #UD, paging, #PF, V86 x2, #GP, RM return\r\n$"
busy_text: .asciz "K386 PROBE REFUSED: PE/V86 owner present; boot without EMM386\r\n$"
failed_text: .asciz "K386 PROBE FAIL\r\n$"
.section .bss
.align 16
tss: .space 104
pages: .space 28672
