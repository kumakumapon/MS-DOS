; COM smoke test: query the actual DOS API and return through INT 21h/4Ch.
.8086
PROBE SEGMENT USE16
ORG 0
    mov ax,3000h
    int 21h
    cmp ax,0004h
    jne failed
    mov dx,OFFSET message+100h
    mov ah,9
    int 21h
    mov ax,4c00h
    int 21h
failed:
    mov dx,OFFSET badversion+100h
    mov ah,9
    int 21h
    mov ax,4c01h
    int 21h
message db 'DOS4 EXEC OK',13,10,'$'
badversion db 'DOS4 VERSION API FAILED',13,10,'$'
PROBE ENDS
END
