.intel_syntax noprefix
.code32
.section .text.start
.global _start
_start:
    cld
    call main
    mov ebx,eax
    xor eax,eax
    int 0x30
    ud2
.section .note.GNU-stack,"",@progbits
