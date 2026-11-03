.intel_syntax noprefix
.code32
.section .text.start
.global _start
_start:
    cld
    sub esp,8
    push DWORD PTR [0xbfd010]
    push DWORD PTR [0xbfd00c]
    call main
    add esp,16
    mov ebx,eax
    xor eax,eax
    int 0x30
    ud2
.section .note.GNU-stack,"",@progbits
