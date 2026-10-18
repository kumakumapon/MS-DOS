.intel_syntax noprefix
.code16
.global _start
_start:
    mov dx,OFFSET area
    mov ah,0x1a
    int 0x21
    mov dx,OFFSET com_mask
    cmp BYTE PTR [0x82],'E'
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
area: .space 43,0
