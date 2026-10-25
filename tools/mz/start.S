.code16gcc
.section .start,"ax"
.global _start, __mz_reloc_ds
_start:
    cld
    # MZ entry: DS=ES=PSP, SS:SP already point into our separate data segment.
    cli
    movzwl %sp,%esp
    sti
    mov $0x4a00,%ax
    mov $__allocation_paras,%bx
    int $0x21
    jc failed
    # DOS relocates this segment word relative to the load-module base.
    .byte 0xb8
__mz_reloc_ds:
    .word __data_segment
    mov %ax,%ds
    mov %ax,%es
    mov $__bss_start,%edi
    mov $__bss_end,%ecx
    sub %edi,%ecx
    xor %eax,%eax
    rep stosb
    call main
    mov $0x4c,%ah
    int $0x21
failed:
    mov $0x4c08,%ax
    int $0x21
.section .note.GNU-stack,"",@progbits
