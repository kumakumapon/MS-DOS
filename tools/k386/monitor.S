.intel_syntax noprefix
.code16gcc
.section .text
.global _start,pm_run
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
    jc unavailable
    xor eax,eax
    mov edi,OFFSET __bss_start
    mov ecx,OFFSET __bss_end
    sub ecx,edi
    rep stosb
    smsw ax
    test al,1
    jnz unavailable
    in al,2
    test al,1
    jz unavailable
    mov ax,0xd386
    xor bx,bx
    int 0x2f
    cmp ax,0x386d
    je already
    call monitor_init
    test eax,eax
    jz unavailable
    mov ax,0x352f
    int 0x21
    mov WORD PTR old_int,bx
    mov WORD PTR old_int+2,es
    mov dx,OFFSET api
    mov ax,0x252f
    int 0x21
    mov dx,OFFSET pass_text
    mov ah,9
    int 0x21
    mov dx,0x1000
    mov ax,0x3100
    int 0x21
already:
    mov ax,0x4c00
    int 0x21
unavailable:
    mov dx,OFFSET busy_text
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
api:
    cmp ax,0xd386
    jne chain
    test bx,bx
    jnz dispatch
    mov ax,cs
    mov es,ax
    mov di,OFFSET k386_mailbox
    mov ax,0x386d
    iretw
chain:
    .byte 0x2e,0xff,0x2e
    .word old_int
dispatch:
    pushad
    pushw ds
    pushw es
    pushw fs
    pushw gs
    mov WORD PTR cs:caller_ss,ss
    mov DWORD PTR cs:caller_esp,esp
    mov ax,cs
    mov ss,ax
    mov ds,ax
    mov es,ax
    mov esp,0xfff0
    sti
    call api_dispatch
    cli
    mov ss,WORD PTR caller_ss
    mov esp,DWORD PTR caller_esp
    popw gs
    popw fs
    popw es
    popw ds
    popad
    iretw
pm_run:
    pushad
    pushfd
    pop DWORD PTR saved_flags
    cli
    mov DWORD PTR real_esp,esp
    mov ax,cs
    mov WORD PTR real_jump+3,ax
    movzx eax,ax
    shl eax,4
    mov DWORD PTR monitor_base,eax
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
    mov eax,DWORD PTR monitor_base
    add eax,OFFSET gdt
    mov DWORD PTR gdtr+2,eax
    mov eax,DWORD PTR monitor_base
    add eax,OFFSET idt
    mov DWORD PTR idtr+2,eax
    sgdt saved_gdtr
    sidt saved_idtr
    mov eax,cr0
    mov DWORD PTR saved_cr0,eax
    mov eax,cr3
    mov DWORD PTR saved_cr3,eax
    lgdt gdtr
    lidt idtr
    mov eax,cr0
    or eax,1
    mov cr0,eax
    .byte 0x66,0xea
    .long pm_start
    .word 8
.code32
pm_start:
    mov ax,16
    mov ds,ax
    mov ss,ax
    mov es,ax
    mov ax,24
    mov fs,ax
    mov esp,0xeff0
    mov DWORD PTR tss+4,0xeff0
    mov WORD PTR tss+8,16
    mov WORD PTR tss+102,104
    mov BYTE PTR gdt+53,0x89
    mov ax,48
    ltr ax
    mov ebx,DWORD PTR selected
    mov eax,DWORD PTR page_directory[ebx*4]
    mov cr3,eax
    mov eax,cr0
    or eax,0x80000000
    mov cr0,eax
    /* Existing host IRQ0 must be masked. Own a bounded one-shot quantum. */
    in al,2
    mov BYTE PTR old_mask,al
    mov al,0xfe
    out 2,al
    mov al,0x30
    out 0x77,al
    xor al,al
    out 0x71,al
    mov al,0x10
    out 0x71,al
    mov eax,DWORD PTR selected
    imul eax,144
    add eax,OFFSET sessions
    /* All 19 saved words share the architectural interrupt-frame layout. */
    push DWORD PTR [eax+72]
    push DWORD PTR [eax+68]
    push DWORD PTR [eax+64]
    push DWORD PTR [eax+60]
    push DWORD PTR [eax+56]
    push DWORD PTR [eax+52]
    push DWORD PTR [eax+48]
    push DWORD PTR [eax+44]
    push DWORD PTR [eax+40]
    push DWORD PTR [eax+28]
    push DWORD PTR [eax+24]
    push DWORD PTR [eax+20]
    push DWORD PTR [eax+16]
    push 0
    push DWORD PTR [eax+8]
    push DWORD PTR [eax+4]
    push DWORD PTR [eax]
    popad
    iretd
irq_or_double:
    test DWORD PTR [esp+8],0x20000
    jz double_fault
    push 0
    push 32
    jmp exception
double_fault:
    push 8
    jmp exception
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
    mov es,ax
    mov ax,24
    mov fs,ax
    push esp
    call kernel_trap
    add esp,4
    test eax,eax
    jnz leave_pm
    popad
    add esp,8
    iretd
leave_pm:
    cli
    mov esp,0xeff0
    call kernel_finish
    mov al,BYTE PTR old_mask
    out 2,al
    mov al,0x60
    out 0,al
    mov eax,cr0
    and eax,0x7fffffff
    mov cr0,eax
    mov eax,DWORD PTR saved_cr3
    mov cr3,eax
    .byte 0xea
    .long pm16
    .word 32
.code16gcc
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
    mov esp,DWORD PTR real_esp
    lidt saved_idtr
    lgdt saved_gdtr
    push DWORD PTR saved_flags
    popfd
    popad
    ret
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
saved_flags: .long 0
real_esp: .long 0
old_mask: .byte 0
caller_ss: .word 0
caller_esp: .long 0
old_int: .word 0,0
.align 8
idt:
.rept 6
    .quad 0
.endr
    .word ud_gate,8
    .byte 0,0x8e
    .word 0
    .quad 0
    .word irq_or_double,8
    .byte 0,0x8e
    .word 0
.rept 4
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
pass_text: .asciz "K386 resident: experimental V86 COM sessions; read-only files\r\n$"
busy_text: .asciz "K386 unavailable: protected-mode or timer owner present\r\n$"
.section .bss
.align 16
tss: .space 104
.section .note.GNU-stack,"",@progbits
