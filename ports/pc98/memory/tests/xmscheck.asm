; Guest-side XMS smoke test for the PC-98 memory drivers.
.386
code segment use16
assume cs:code, ds:code, es:code
org 100h

start:
    cld
    push cs
    pop ds
    push cs
    pop es

    mov ax,4300h
    int 2fh
    cmp al,80h
    jne fail_detect

    mov ax,4310h
    int 2fh
    mov word ptr [xms_entry],bx
    mov word ptr [xms_entry+2],es

    xor ax,ax
    call xms_call
    or ax,ax
    jz fail_version

    mov ah,08h
    call xms_call
    or dx,dx
    jz fail_free

    mov ah,09h
    mov dx,64
    call xms_call
    cmp ax,1
    jne fail_alloc
    mov [xms_handle],dx

    ; Copy the known string into XMS, overwrite it in conventional RAM, then
    ; move it back. This exercises XMS function 0Bh as well as allocation.
    mov dword ptr [move_block],14
    mov word ptr [move_block+4],0
    mov word ptr [move_block+6],offset test_data
    mov word ptr [move_block+8],cs
    mov ax,[xms_handle]
    mov word ptr [move_block+10],ax
    mov dword ptr [move_block+12],0
    mov si,offset move_block
    mov ah,0bh
    call xms_call
    cmp ax,1
    jne fail_move_to

    mov byte ptr [test_data],0
    mov dword ptr [move_block+6],0
    mov ax,[xms_handle]
    mov word ptr [move_block+4],ax
    mov word ptr [move_block+10],0
    mov word ptr [move_block+12],offset test_data
    mov word ptr [move_block+14],cs
    mov si,offset move_block
    mov ah,0bh
    call xms_call
    cmp ax,1
    jne fail_move_from

    push cs
    pop ds
    push cs
    pop es
    mov si,offset test_data
    mov di,offset expected_data
    mov cx,14
    repe cmpsb
    jne fail_compare

    mov ah,0ah
    mov dx,[xms_handle]
    call xms_call
    cmp ax,1
    jne fail_free_handle

    mov dx,offset pass_message
    mov ah,09h
    int 21h
    mov ax,4c00h
    int 21h

fail_detect:
    mov dx,offset detect_message
    jmp fail_print
fail_version:
    mov dx,offset version_message
    jmp fail_print
fail_free:
    mov dx,offset free_message
    jmp fail_print
fail_alloc:
    mov dx,offset alloc_message
    jmp fail_print
fail_move_to:
    mov word ptr [fail_pointer],offset move_to_message
    jmp release_and_print
fail_move_from:
    mov word ptr [fail_pointer],offset move_from_message
    jmp release_and_print
fail_compare:
    mov word ptr [fail_pointer],offset compare_message
    jmp release_and_print
fail_free_handle:
    mov dx,offset free_handle_message
    jmp fail_print
release_and_print:
    mov ah,0ah
    mov dx,[xms_handle]
    call xms_call
    mov dx,[fail_pointer]
fail_print:
    mov ah,09h
    int 21h
fail_exit:
    mov ax,4c01h
    int 21h

xms_call proc near
    call dword ptr [xms_entry]
    ret
xms_call endp

xms_entry    dd 0
xms_handle   dw 0
move_block   db 16 dup (0)
fail_pointer dw 0
test_data    db 'XMS-API-CHECK!'
expected_data db 'XMS-API-CHECK!'
pass_message db 'XMS allocation/move/free OK',13,10,'$'
detect_message db 'XMS not detected',13,10,'$'
version_message db 'XMS version query failed',13,10,'$'
free_message db 'XMS free-memory query failed',13,10,'$'
alloc_message db 'XMS allocation failed',13,10,'$'
move_to_message db 'XMS move to extended memory failed',13,10,'$'
move_from_message db 'XMS move to conventional memory failed',13,10,'$'
compare_message db 'XMS move data mismatch',13,10,'$'
free_handle_message db 'XMS handle free failed',13,10,'$'

code ends
end start
