.intel_syntax noprefix
.code16
.global _start
_start:
    /* Zero-length DOS input at the final two private bytes must return safely. */
    mov BYTE PTR [0xfffe],0
    mov dx,0xfffe
    mov ah,0x0a
    int 0x21
    std
    mov dx,OFFSET text
    mov ah,9
    int 0x21
    cld
    mov ax,0x4c00
    int 0x21
text: .ascii "V86 DIRECTION FLAG PASS\r\n$"
