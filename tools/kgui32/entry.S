/* Native loader entry. No KDESK code bank; the bounded graphical callback is transient. */
.code16gcc
.global initialized
_start:
    cld
    cli
    mov ax,cs
    mov ds,ax
    mov ss,ax
    mov esp,0xfff0
    sti
    mov bx,0x1010
    mov ax,0x4a00
    int 0x21
    jc gui_entry_failed
    mov ax,ds
    mov es,ax
    xor eax,eax
    mov edi,OFFSET __bss_start
    mov ecx,OFFSET __bss_end
    sub ecx,edi
    rep stosb
    call gui_main
    mov ah,0x4c
    int 0x21
gui_entry_failed:
    mov ax,0x4c08
    int 0x21
.include "host-engine.S"
.section .text
.code16
