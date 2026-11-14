/* Original V86 diagnostic. Only actual guest OUT/IN and private CPU stores. */
.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
    cmp BYTE PTR [0x80],0
    je first
    cmp BYTE PTR [0x82],'0'
    jne variant
    mov BYTE PTR skipfont,1
variant:
    cmp BYTE PTR [0x82],'2'
    jne first
    mov BYTE PTR pattern,0x42
first:
    mov dx,OFFSET banner
    mov ah,9
    int 0x21
    mov ax,0xa000
    mov es,ax
    xor di,di
    mov ax,32
    mov cx,2000
    rep stosw
    mov di,0x2000
    mov ax,0xe1
    mov cx,2000
    rep stosw
    mov WORD PTR es:[0],0xa156
    mov WORD PTR es:[2],0xa156
    mov WORD PTR es:[160],0xa5
    mov WORD PTR es:[160+0x2000],0xf1
    cmp BYTE PTR skipfont,1
    je wait
    mov dx,0xa1
    mov al,0x21
    out dx,al
    mov dx,0xa3
    mov al,0x56
    out dx,al
    xor bx,bx
font:
    mov dx,0xa5
    mov ax,bx
    out dx,al
    mov dx,0xa9
    mov al,BYTE PTR pattern
    out dx,al
    inc bx
    cmp bx,48
    jne font
    mov dx,0x68
    mov al,1
    out dx,al
wait:
    mov ah,8
    int 0x21
    cmp al,'q'
    je done
    cmp al,'f'
    je fault
    cmp al,'a'
    je analog
    cmp al,'g'
    je graphics
    cmp al,'4'
    je forty
    cmp al,'8'
    je eighty
    cmp al,'t'
    je textmode
    cmp al,'n'
    jne wait
    mov dx,0x68
    xor al,al
    out dx,al
    jmp wait
analog:
    mov dx,0x6a
    mov al,1
    out dx,al
    mov dx,0xa8
    out dx,al
    mov dx,0xaa
    mov al,12
    out dx,al
    mov dx,0xac
    mov al,3
    out dx,al
    mov dx,0xae
    mov al,6
    out dx,al
    jmp wait
graphics:
    mov ax,0xa800
    mov es,ax
    mov ax,0xaaaa
    call fill
    mov ax,0xb000
    mov es,ax
    mov ax,0xcccc
    call fill
    mov ax,0xb800
    mov es,ax
    mov ax,0xf0f0
    call fill
    mov ax,0xe000
    mov es,ax
    mov ax,0xff00
    call fill
    mov dx,0x62
    mov al,12
    out dx,al
    mov dx,0xa2
    mov al,13
    out dx,al
    /* Exercise the GDC command FIFO, CURS/MASK and bounded word WDAT. */
    mov al,0x49
    out dx,al
    mov dx,0xa0
    xor al,al
    out dx,al
    out dx,al
    out dx,al
    mov dx,0xa2
    mov al,0x20
    out dx,al
    mov dx,0xa0
    mov al,0xff
    out dx,al
    out dx,al
    /* Real IN keeps the upper EAX bytes and reports FIFO empty. */
    mov eax,0x12345600
    in al,dx
    test al,4
    jz fault
    and eax,0xffffff00
    cmp eax,0x12345600
    jne fault
    jmp wait
fill:
    xor di,di
    mov cx,16384
    rep stosw
    ret
textmode:
    mov dx,0xa2
    mov al,12
    out dx,al
    mov dx,0x62
    mov al,13
    out dx,al
    jmp wait
forty:
    mov BYTE PTR sync+1,38
    mov BYTE PTR raster,0x93
    mov BYTE PTR pitch,40
    jmp mode
eighty:
    mov BYTE PTR sync+1,78
    mov BYTE PTR raster,0x8f
    mov BYTE PTR pitch,80
mode:
    mov dx,0x62
    mov al,14
    out dx,al
    mov si,OFFSET sync
    mov cx,8
    mov dx,0x60
modebyte:
    lodsb
    out dx,al
    loop modebyte
    mov dx,0x62
    mov al,0x47
    out dx,al
    mov dx,0x60
    mov al,BYTE PTR pitch
    out dx,al
    mov dx,0x62
    mov al,0x4b
    out dx,al
    mov dx,0x60
    mov al,BYTE PTR raster
    out dx,al
    xor al,al
    out dx,al
    out dx,al
    jmp wait
fault:
    mov dx,0xa2
    out dx,ax
done:
    mov ax,0x4c00
    int 0x21
pattern: .byte 0x81
skipfont: .byte 0
pitch: .byte 80
raster: .byte 0x8f
sync: .byte 0,78,0,0,0,0,0x90,1
banner: .ascii "KVTERM READY: g graphics, t text, 4/8 modes, n normal, f fault, q quit\r\n$"
