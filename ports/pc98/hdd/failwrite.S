# Diagnostic TSR for disposable images only. Deny exactly one HDD BIOS write.
# Never loaded by the normal AUTOEXEC. FD BIOS operations are unaffected.
.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
 mov ax,0x351b
 int 0x21
 mov WORD PTR cs:previous,bx
 mov WORD PTR cs:previous+2,es
 mov dx,OFFSET handler
 mov ax,0x251b
 int 0x21
 mov dx,OFFSET resident_paras
 mov ax,0x3100
 int 0x21
handler:
 cmp ax,0x0500
 jne chain
 cmp BYTE PTR cs:remaining,0
 je chain
 dec BYTE PTR cs:remaining
 mov ah,0x60
 push bp
 mov bp,sp
 or WORD PTR ss:[bp+6],1
 pop bp
 iretw
chain:
 .byte 0x2e,0xff,0x2e
 .word previous
previous:.long 0
remaining:.byte 1
resident_end:
.section .note.GNU-stack,"",@progbits
