.intel_syntax noprefix
.code16
.global _start
_start:
    mov DWORD PTR [0x800],0
    cli
    pushf
    pop ax
    test ax,0x200
    jnz fail
    or ax,0x200
    push ax
    popf
    pushfd
    pop eax
    test eax,0x200
    jz fail
    or eax,0x7000
    push eax
    popfd
    pushfd
    pop eax
    test eax,0x7000
    jnz fail
    mov dx,OFFSET handler
    mov ax,0x2560
    int 0x21
    int 0x60
    cmp ax,0xbeef
    jne fail
    mov dx,OFFSET handler32
    mov ax,0x2561
    int 0x21
    .byte 0x66,0xcd,0x61
    cmp ax,0xcafe
    jne fail
    cli
    mov cx,0xffff
count:
    inc DWORD PTR [0x800]
    loop count
    sti
    mov dx,OFFSET success
    mov ah,9
    int 0x21
    mov ax,0x4c00
    int 0x21
fail:
    mov dx,OFFSET failure
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
handler:
    mov ax,0xbeef
    iret
handler32:
    mov ax,0xcafe
    iretd
success: .ascii "V86 FLAGS AND IRET PASS\r\n$"
failure: .ascii "V86 FLAGS FAIL\r\n$"
