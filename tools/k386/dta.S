.intel_syntax noprefix
.code16
.global _start
_start:
.ifndef K386_APP
    push ds
    pop fs
.endif
    mov dx,OFFSET area
    mov ah,0x1a
    int 0x21
    mov dx,OFFSET com_mask
    cmp BYTE PTR fs:[0x82],'E'
    jne first
    mov dx,OFFSET exe_mask
first:
    mov ax,0x4e00
    mov cx,0x10
    int 0x21
    jc fail
    mov dx,OFFSET initial
    call print
    call filename
    # Modified drive/network/cluster continuation bytes must not reach DOS.
    mov al,BYTE PTR [area]
    mov BYTE PTR [saved_drive],al
    mov BYTE PTR [area],2
    call rejected
    mov BYTE PTR [area],0x81
    call rejected
    mov al,BYTE PTR [saved_drive]
    mov BYTE PTR [area],al
    not BYTE PTR [area+15]
    call rejected
    not BYTE PTR [area+15]
    mov dx,OFFSET guarded
    call print
    mov ah,8
    int 0x21
    mov ah,0x4f
    int 0x21
    jc fail
    mov dx,OFFSET next
    call print
    call filename
    mov dx,OFFSET success
    call print
    mov ax,0x4c00
    int 0x21
fail:
    mov dx,OFFSET failure
    call print
    mov ax,0x4c01
    int 0x21
print:
    mov ah,9
    int 0x21
    ret
rejected:
    mov ah,0x4f
    int 0x21
    jnc fail
    cmp ax,18
    jne fail
    ret
filename:
    mov dx,OFFSET area+30
    mov si,dx
    xor cx,cx
scan:
    cmp BYTE PTR [si],0
    je output
    inc si
    inc cx
    jmp scan
output:
    mov bx,1
    mov ah,0x40
    int 0x21
    mov dx,OFFSET newline
    call print
    ret
com_mask: .asciz "*.COM"
exe_mask: .asciz "*.EXE"
initial: .ascii "DTA FIRST: $"
next: .ascii "DTA NEXT: $"
newline: .ascii "\r\n$"
success: .ascii "DTA PRIVATE SEARCH PASS\r\n$"
failure: .ascii "DTA SEARCH FAIL\r\n$"
guarded: .ascii "DTA RESERVED GUARD PASS\r\n$"
saved_drive: .byte 0
area: .space 43,0
