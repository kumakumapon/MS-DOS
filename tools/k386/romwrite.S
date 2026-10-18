/* Readable firmware must still reject V86 writes with a real CPU #PF. */
.intel_syntax noprefix
.code16
.global _start
_start:
    mov ax,0xf000
    mov es,ax
    mov BYTE PTR es:[0xfffe],0
    mov ax,0x4c7f
    int 0x21
