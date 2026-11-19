/* Real V86 instructions exercise BIOS, word I/O and two private full pages. */
.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
    cmp BYTE PTR [0x80],0
    je initialize
    cmp BYTE PTR [0x82],'0'
    je setup
initialize:
    mov dx,0xa6
    xor ax,ax
    out dx,ax
    call first_page
    mov dx,0xa6
    mov ax,1
    out dx,ax
    call second_page
setup:
    mov ax,0x4200
    mov cx,0xc000
    int 0x18
    jc failed
    mov ax,0x0d00
    int 0x18
    mov ax,0x4000
    int 0x18
    mov dx,0xa0
    mov eax,0x1234ffff
    in ax,dx
    test ah,ah
    jnz failed
    shr eax,16
    cmp ax,0x1234
    jne failed
wait:
    mov ax,0x0100
    int 0x18
    test bh,bh
    jz yield
    mov ax,0x0500
    int 0x18
    cmp bh,1
    jne failed
    cmp al,'q'
    je done
    cmp al,'f'
    je fault
    cmp al,'0'
    je page0
    cmp al,'1'
    je page1
    cmp al,'t'
    je text
    cmp al,'g'
    je graph
    cmp al,'a'
    je modify
    cmp al,'b'
    je badbios
    cmp al,'h'
    je flush_read
    cmp al,'k'
    jne wait
    mov ax,0x0300
    int 0x18
    mov ax,0x0200
    int 0x18
    test al,al
    jnz failed
    jmp wait
flush_read:
    mov ax,0x0c06
    mov dl,0xff
    int 0x21
    jnz failed
    mov ax,0x0a02
    int 0x18
    mov ax,0x0c07
    int 0x21
    cmp al,'z'
    jne failed
    mov ax,0x0a00
    int 0x18
    jmp wait
badbios:
    mov ax,0x0aff
    int 0x18
    jnc failed
    cmp al,38
    jne failed
    jmp wait
yield:
    mov ax,0xd388
    mov bx,1
    int 0x2f
    jmp wait
page0:
    mov dx,0xa4
    xor ax,ax
    out dx,ax
    jmp wait
page1:
    mov dx,0xa4
    mov ax,1
    out dx,ax
    jmp wait
modify:
    mov dx,0xa6
    xor ax,ax
    out dx,ax
    mov ax,0xa800
    mov es,ax
    mov WORD PTR es:[0],0
    jmp wait
text:
    mov ax,0x4100
    int 0x18
    mov ax,0x0a01
    int 0x18
    mov ax,0x0b00
    int 0x18
    cmp al,1
    jne failed
    mov ax,0x1600
    mov dx,0xe12a
    int 0x18
    mov ax,0x0c00
    int 0x18
    mov ax,0x1300
    mov dx,242
    int 0x18
    mov ax,0x1100
    int 0x18
    jmp wait
graph:
    mov ax,0x0d00
    int 0x18
    mov ax,0x4000
    int 0x18
    jmp wait
fault:
    mov dx,0xa2
    out dx,eax
failed:
    mov ax,0x4c63
    int 0x21
done:
    mov ax,0x4c00
    int 0x21
first_page:
    mov ax,0xa800
    mov es,ax
    mov ax,0xffff
    call fill
    mov ax,0xb000
    mov es,ax
    xor ax,ax
    call fill
    mov ax,0xb800
    mov es,ax
    xor ax,ax
    call fill
    mov ax,0xe000
    mov es,ax
    xor ax,ax
    call fill
    ret
second_page:
    mov ax,0xa800
    mov es,ax
    xor ax,ax
    call fill
    mov ax,0xb000
    mov es,ax
    xor ax,ax
    call fill
    mov ax,0xb800
    mov es,ax
    mov ax,0xffff
    call fill
    mov ax,0xe000
    mov es,ax
    mov ax,0xa5a5
    call fill
    ret
fill:
    xor di,di
    mov cx,16000
    cld
    rep stosw
    ret
