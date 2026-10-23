/* Shared startup for a flat 16-bit KDESK/386 text application. */
.code16gcc
.section .start,"ax"
.global _start, __mz_reloc_ds
_start:
    cld
    mov %ds,%ax
    mov %ax,%fs                 # explicit PSP access for assembly applications
    movzwl %sp,%esp
    mov $0xd388,%ax
    xor %bx,%bx
    int $0x2f
    cmp $0x388d,%ax
    jne unavailable
    cmp $0x0100,%bx
    jne unavailable
    .byte 0xb8
__mz_reloc_ds:
    .word 0
    mov %ax,%ds
    mov %ax,%es
    mov $__bss_start,%edi
    mov $__bss_end,%ecx
    sub %edi,%ecx
    xor %eax,%eax
    rep stosb
#ifdef KAPP_C
    call main
    mov $0x4c,%ah
    int $0x21
#else
    jmp app_main
#endif
unavailable:
    pushw %cs
    popw %ds
    mov $required,%dx
    mov $9,%ah
    int $0x21
    mov $0x4c01,%ax
    int $0x21
required: .ascii "Open this application in KDESK/386.\r\n$"
.section .note.GNU-stack,"",@progbits
