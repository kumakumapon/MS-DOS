.intel_syntax noprefix
.code16
.global _start
_start:
    /* 6521h uppercases CX bytes, including bytes after a NUL. */
    mov dx,OFFSET counted
    mov cx,5
    mov ax,0x6521
    int 0x21
    jc fail
    /* 6522h stops at the NUL and ignores CX. */
    mov dx,OFFSET asciz
    xor cx,cx
    mov ax,0x6522
    int 0x21
    jc fail
    cmp WORD PTR counted,0x4241
    jne fail
    cmp WORD PTR counted+3,0x4443
    jne fail
    cmp WORD PTR asciz,0x5958
    jne fail
    cmp BYTE PTR asciz+3,'z'
    jne fail
    mov dx,OFFSET pass_text
    jmp done
fail:
    mov dx,OFFSET fail_text
done:
    mov ah,9
    int 0x21
    mov ax,0x4c00
    int 0x21
counted: .byte 'a','b',0,'c','d'
asciz: .byte 'x','y',0,'z',0
pass_text: .ascii "CASEMAP PASS\r\n$"
fail_text: .ascii "CASEMAP FAIL\r\n$"
