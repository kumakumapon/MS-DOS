.code16gcc
.section .start,"ax"
.global _start
_start:
    cld
    cli
    mov %cs,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%ss
    movzwl %sp,%esp
    sti
    mov 2,%bx
    sub %ax,%bx
    cmp $0x1000,%bx
    jb failed
    mov $0xfff0,%esp
    # A COM receives the largest free block; release memory for child EXECs.
    mov $0x4a00,%ax
    mov $0x1000,%bx
    int $0x21
    jc failed
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
