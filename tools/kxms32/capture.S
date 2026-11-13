/* Save/reserve the BIOS high pool before the 286 manager clears its size. */
.intel_syntax noprefix
.code16
.section .text
.long 0xffffffff
.word 0x8000
.word capture_strategy,capture_interrupt
.ascii "KXHIGH$ "
.global capture_strategy
capture_strategy:
 mov WORD PTR cs:request,bx
 mov WORD PTR cs:request+2,es
 retf
capture_interrupt:
 pushf
 pusha
 push ds
 push es
 push cs
 pop ds
 les bx,DWORD PTR request
 mov WORD PTR es:[bx+3],0x100
 cmp BYTE PTR es:[bx+2],0
 jne capture_done
 mov WORD PTR es:[bx+14],OFFSET capture_end
 mov WORD PTR es:[bx+16],cs
 cmp BYTE PTR seen,0
 jne capture_done
 mov BYTE PTR seen,1
 mov ax,0xd38d
 int 0x2f
 cmp ax,0x38dd
 je capture_done
 mov ax,0x4300
 int 0x2f
 cmp al,0x80
 je capture_done
 xor ax,ax
 mov es,ax
 mov ax,WORD PTR es:[0x594]
 cmp ax,16
 jb capture_done
 mov WORD PTR captured+12,ax
 mov ax,0x352f
 int 0x21
 mov WORD PTR old_vector,bx
 mov WORD PTR old_vector+2,es
 mov dx,OFFSET capture_api
 mov ax,0x252f
 int 0x21
 xor ax,ax
 mov es,ax
 mov WORD PTR es:[0x594],0
capture_done:
 pop es
 pop ds
 popa
 popf
 retf
capture_api:
 cmp ax,0xd38d
 jne capture_chain
 mov ax,cs
 mov es,ax
 mov di,OFFSET captured
 mov ax,0x38dd
 iret
capture_chain:
 .byte 0x2e,0xff,0x2e
 .word old_vector
request:.long 0
old_vector:.long 0
seen:.byte 0
captured:
.ascii "KX32BOOT"
.word 1,16,0,0
capture_end:
.section .note.GNU-stack,"",@progbits
