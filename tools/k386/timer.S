/* PC-98 INT 1Ch one-shot interval callback while DOS input is waiting. */
.intel_syntax noprefix
.code16
.global _start
_start:
    mov ax,cs
    mov es,ax
    mov bx,OFFSET callback
    mov cx,2
    mov ah,2
    int 0x1c
    cli
    mov cx,8
outer:
    push cx
    mov cx,0xffff
busy:
    loop busy
    pop cx
    loop outer
    cmp DWORD PTR [0x800],0
    jne fail
    sti
    cmp BYTE PTR [0x80],0
    je waiting_for_key
    mov ax,cs
    mov WORD PTR [parameters+4],ax
    mov WORD PTR [parameters+8],ax
    mov WORD PTR [parameters+12],ax
    mov bx,OFFSET parameters
    mov dx,OFFSET child
    mov ax,0x4b00
    int 0x21
    jc fail
waiting_for_key:
    mov dx,OFFSET waiting
    mov ah,9
    int 0x21
    mov ah,8
    int 0x21
    cmp DWORD PTR [0x800],3
    jb fail
    cmp BYTE PTR [0x80],0
    jne success_result
    cmp DWORD PTR [0x800],3
    jne fail
success_result:
    mov dx,OFFSET success
    mov ah,9
    int 0x21
    mov ax,0x4c00
    int 0x21
callback:
    push ax
    push ds
    push bx
    push cx
    push es
    mov ax,cs
    mov ds,ax
    inc DWORD PTR [0x800]
    cmp BYTE PTR [0x80],0
    jne rearm
    cmp DWORD PTR [0x800],3
    jae done
rearm:
    xor ax,ax
    mov es,ax
    mov WORD PTR es:[0x58a],2
    mov ah,3
    int 0x1c
done:
    pop es
    pop cx
    pop bx
    pop ds
    pop ax
    iret
fail:
    mov dx,OFFSET failure
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
waiting: .ascii "V86 TIMER WAIT: callbacks continue without input\r\n$"
success: .ascii "V86 PC98 TIMER PASS\r\n$"
failure: .ascii "V86 TIMER FAIL\r\n$"
child: .asciz "COUNTER.COM"
tail: .byte 0,13
fcb: .space 37
parameters: .word 0,tail,0,fcb,0,fcb,0
