.intel_syntax noprefix
.code16
.global _start
_start:
    fldz /* No FPU context sharing: the monitor must contain #NM. */
    mov ax,0x4c01
    int 0x21
