/* Four real EXEC contexts, inherited environments/handles and private IVTs. */
.intel_syntax noprefix
.code16
.global _start
_start:
.ifndef K386_APP
    push ds
    pop fs
.endif
    mov sp,0x8000
    mov ax,fs
    mov es,ax
    mov bx,0x900
    mov ah,0x4a
    int 0x21
    jc fail
    mov ah,0x62
    int 0x21
    mov WORD PTR [psp],bx
    mov BYTE PTR [level],0
    cmp BYTE PTR fs:[0x80],0
    je root
    mov al,BYTE PTR fs:[0x82]
    sub al,'0'
    cmp al,3
    ja fail
    mov BYTE PTR [level],al
    mov ax,WORD PTR fs:[0x2c]
    mov dx,ax
    mov es,ax
    cmp DWORD PTR es:[0],0x424f5250 /* PROB */
    jne fail
    cmp DWORD PTR es:[4],0x52503d45 /* E=PR */
    jne fail
    mov ax,WORD PTR fs:[0x16]
    cmp ax,WORD PTR [psp]
    je fail
    mov es,ax
    cmp dx,WORD PTR es:[0x2c]
    je fail
    mov ah,0x49
    int 0x21
    jnc fail
    cmp ax,9
    jne fail
    jmp common
root:
    mov ax,0x3d00
    mov dx,OFFSET datafile
    int 0x21
    jc fail
    cmp ax,5
    jne fail
    mov bx,5
    mov ah,0x45
    int 0x21
    jc fail
    mov bx,ax
    mov ah,0x3e
    int 0x21
    jc fail
    mov bx,1
    mov cx,11
    mov ah,0x46
    int 0x21
    jc fail
common:
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ah,0x1a
    mov dx,OFFSET dta
    int 0x21
    mov ax,0x2560
    mov dx,OFFSET callback
    int 0x21
    mov bx,5
    mov cx,1
    mov dx,OFFSET byte
    mov ah,0x3f
    int 0x21
    jc fail
    cmp ax,1
    jne fail
    xor bx,bx
    mov bl,BYTE PTR [level]
    mov al,BYTE PTR [letters+bx]
    cmp BYTE PTR [byte],al
    jne fail
    mov WORD PTR [0x900],0x5aa5
    mov ax,cs
    mov bx,OFFSET environment
    shr bx,4
    add ax,bx
    mov WORD PTR [parameters],ax
    mov ax,cs
    mov WORD PTR [parameters+4],ax
    mov WORD PTR [parameters+8],ax
    mov WORD PTR [parameters+12],ax
    mov al,BYTE PTR [level]
    add al,'1'
    mov BYTE PTR [tail+2],al
    mov ax,0x4b00
    mov dx,OFFSET program
    mov bx,OFFSET parameters
    int 0x21
    pushf
    cmp BYTE PTR [level],3
    je deepest
    popf
    jc fail
    mov ah,0x4d
    int 0x21
    mov bl,BYTE PTR [level]
    inc bl
    cmp al,bl
    jne fail
    test ah,ah
    jnz fail
    jmp restored
deepest:
    popf
    jnc fail
    cmp ax,8
    jne fail
restored:
    cmp WORD PTR [0x900],0x5aa5
    jne fail
    int 0x60
    cmp ah,0xca
    jne fail
    cmp al,BYTE PTR [level]
    jne fail
    mov ah,0x2f
    int 0x21
    cmp bx,OFFSET dta
    jne fail
    mov ax,es
    mov dx,cs
    cmp ax,dx
    jne fail
    cmp BYTE PTR [level],0
    jne exit
    mov bx,5
    mov cx,1
    mov dx,OFFSET byte
    mov ah,0x3f
    int 0x21
    jc fail
    cmp BYTE PTR [byte],' '
    jne fail
    mov bx,11
    mov cx,success_end-success
    mov dx,OFFSET success
    mov ah,0x40
    int 0x21
    jc fail
exit:
    mov al,BYTE PTR [level]
    mov ah,0x4c
    int 0x21
callback:
    mov ah,0xca
    mov al,BYTE PTR cs:[level]
    iret
fail:
    mov dx,OFFSET failure
    mov ah,9
    int 0x21
    mov ax,0x4c7f
    int 0x21
psp: .word 0
level: .byte 0
byte: .byte 0
letters: .ascii "READ"
datafile: .asciz "KDATA.TXT"
program:
.ifdef K386_APP
    .asciz "PROCESS.EXE"
.else
    .asciz "PROCESS.COM"
.endif
tail: .byte 2,' ','1',13
parameters: .word 0,tail,0,dta,0,dta,0
dta: .space 43
success: .ascii "V86 NESTED PROCESS PASS: PSP/env/MCB/JFT/IVT x4\r\n"
success_end:
failure: .ascii "V86 NESTED PROCESS FAIL\r\n$"
.balign 16
environment: .asciz "PROBE=PROCESS\0\0"
