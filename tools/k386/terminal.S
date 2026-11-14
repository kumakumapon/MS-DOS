/* Original read-only DOS diagnostic; all TVRAM writes are private under V86. */
.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
    mov ax,0x0c00
    int 0x18
    mov dx,OFFSET banner
    call puts
    mov dx,OFFSET japanese
    call puts
    mov dx,OFFSET split
    call puts
    mov ah,2
    mov dl,0x93
    int 0x21
    mov al,0xfa
    int 0x29
    mov dx,OFFSET styles
    call puts
    mov ax,0xa000
    mov es,ax
    mov di,7*160
    mov ax,0xfc26
    mov es:[di],ax
    mov WORD PTR es:[di+2],0
    mov WORD PTR es:[di+0x2000],0x41
    mov WORD PTR es:[di+0x2002],0x81
    mov di,8*160
    mov WORD PTR es:[di],'V'
    mov WORD PTR es:[di+0x2000],0xf1
    mov di,10*160+7*2
    mov WORD PTR es:[di],0xfc26
    mov WORD PTR es:[di+2],0
    mov WORD PTR es:[di+0x2000],0xe1
    mov WORD PTR es:[di+0x2002],0xe1
    mov dx,OFFSET ready
    call puts
wait:
    mov ah,7
    int 0x21
    cmp al,'q'
    je done
    cmp al,'f'
    je fault
    cmp al,'a'
    je attribute
    cmp al,'j'
    je upper
    cmp al,'w'
    je wrap
    cmp al,'s'
    je scroll
    cmp al,'r'
    je cursor
    cmp al,'i'
    je input
    jmp wait
attribute:
    mov ax,0xa000
    mov es,ax
    mov di,2*160+0x2000
    mov cx,6
    mov ax,0x41
    rep stosw
    jmp wait
upper:
    mov ax,0xa000
    mov es,ax
    mov BYTE PTR es:[2*160+1],0xfb /* ku unchanged; 日 -> 匂 */
    jmp wait
wrap:
    mov dx,OFFSET wrap_text
    call puts
    jmp wait
scroll:
    mov cx,30
scroll_loop:
    push cx
    mov dx,OFFSET scroll_text
    call puts
    pop cx
    loop scroll_loop
    jmp wait
cursor:
    mov dx,OFFSET cursor_text
    call puts
    jmp wait
input:
    mov dx,OFFSET input_text
    call puts
    mov dx,OFFSET input_buffer
    mov ah,0x0a
    int 0x21
    mov dx,OFFSET echo_text
    call puts
    xor cx,cx
    mov cl,BYTE PTR input_buffer+1
    mov dx,OFFSET input_buffer+2
    mov bx,1
    mov ah,0x40
    int 0x21
    jmp wait
fault:
    mov ax,0xc000
    mov es,ax
    mov WORD PTR es:[0],0x55aa
done:
    mov ax,0x4c00
    int 0x21
puts:
    mov ah,9
    int 0x21
    ret
banner: .ascii "TERM98 native SJIS / PC-98 text attributes\r\n$"
japanese: .ascii "\033[3;1H"; .byte 0x93,0xfa,0x96,0x7b,0x8c,0xea,32,0xb6,0xc5; .ascii "$"
split: .ascii "\033[4;1H$"
styles: .ascii "\033[5;1H\033[31mRED\033[32mGREEN\033[34mBLUE\033[0mWHITE"
 .ascii "\033[6;1H\033[7mREV\033[0m \033[4mUNDER\033[0m \033[8mHIDE\033[0m"
 .ascii "\033[7;1H\033[5mBLINK\033[0m$"
ready: .ascii "\033[1;1HTERM98 READY\033[10;1H$"
wrap_text: .ascii "\033[25;80H\033[34m"; .byte 0x93,0xfa; .ascii "\033[0m$"
scroll_text: .ascii "\033[31mSCROLL "; .byte 0x93,0xfa; .ascii "\033[0m\r\n$"
cursor_text: .ascii "\033[3;2H$"
input_text: .ascii "\033[20;1H\033[0mINPUT:$"
echo_text: .ascii "\033[21;1HECHO: $"
input_buffer: .byte 32,0; .space 34
