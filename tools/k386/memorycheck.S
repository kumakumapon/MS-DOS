/* Real guest allocator/MCB checks and a >64K allocation kept alive in each VM. */
.intel_syntax noprefix
.code16
.global _start
_start:
    mov ah,0x62
    int 0x21
    mov WORD PTR [psp],bx
    cmp WORD PTR [0x2c],0
    je fail
    mov ah,0x48
    mov bx,0xffff
    int 0x21
    jnc fail
    cmp ax,8
    jne fail
    cmp bx,0x1400
    jb fail
    mov bx,0x1200
    mov ah,0x48
    int 0x21
    jc fail
    mov WORD PTR [block],ax
    mov es,ax
    mov WORD PTR es:[0],0x1357
    add ax,0x1000
    mov es,ax
    mov WORD PTR es:[0x1ffe],0x2468
    mov ax,WORD PTR [block]
    dec ax
    mov es,ax
    cmp WORD PTR es:[3],0x1200
    jne fail
    mov ax,WORD PTR [psp]
    cmp WORD PTR es:[1],ax
    jne fail
    /* A forged writable MCB must not change the trusted owner/size. */
    mov WORD PTR es:[1],0xdead
    mov WORD PTR es:[3],0xffff
    mov ax,WORD PTR [block]
    mov es,ax
    mov bx,0x1400
    mov ah,0x4a
    int 0x21
    jc fail
    cmp WORD PTR es:[0],0x1357
    jne fail
    mov ax,es
    add ax,0x1000
    mov es,ax
    cmp WORD PTR es:[0x1ffe],0x2468
    jne fail
    mov ax,WORD PTR [block]
    mov es,ax
    mov bx,0x800
    mov ah,0x4a
    int 0x21
    jc fail
    mov ah,0x49
    int 0x21
    jc fail
    mov ah,0x49
    int 0x21
    jnc fail
    cmp ax,9
    jne fail
    mov bx,0x1200
    mov ah,0x48
    int 0x21
    jc fail
    mov WORD PTR [block],ax
    /* Per-instance cookie comes from the command tail (A/B). */
    mov al,BYTE PTR [0x82]
    xor ah,ah
    mov WORD PTR [cookie],ax
    mov bx,WORD PTR [block]
    add bx,0x1000
    mov es,bx
    mov WORD PTR es:[0x1ffe],ax
    mov dx,OFFSET success
    mov ah,9
    int 0x21
running:
    mov ax,WORD PTR [cookie]
    cmp WORD PTR es:[0x1ffe],ax
    jne fail
    inc DWORD PTR [0x800]
    mov ah,0x0b
    int 0x21
    test al,al
    jz running
    mov ah,8
    int 0x21
    mov ax,0x4c00
    int 0x21
fail:
    mov dx,OFFSET failure
    mov ah,9
    int 0x21
    mov ax,0x4c01
    int 0x21
psp: .word 0
block: .word 0
cookie: .word 0
success: .ascii "V86 DOS MEMORY PASS: private >64K block alive\r\n$"
failure: .ascii "V86 DOS MEMORY FAIL\r\n$"
