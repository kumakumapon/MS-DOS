; Guest-side EMS API smoke test for the PC-98 EMM386 driver.
.286
code segment use16
assume cs:code, ds:code, es:nothing
org 100h

start:
    push cs
    pop ds

    mov ah,40h
    int 67h
    or ah,ah
    jnz fail_detect

    mov ah,41h
    int 67h
    or ah,ah
    jnz fail_frame
    mov [page_frame],bx

    mov ah,43h
    mov bx,2
    int 67h
    or ah,ah
    jnz fail_alloc
    mov [ems_handle],dx

    ; Map, write distinct contents, then remap each logical page and verify.
    mov ah,44h
    mov al,0
    xor bx,bx
    mov dx,[ems_handle]
    int 67h
    or ah,ah
    jnz fail_map0
    mov ax,[page_frame]
    mov es,ax
    mov word ptr es:[0],1234h

    mov ah,44h
    mov al,0
    mov bx,1
    mov dx,[ems_handle]
    int 67h
    or ah,ah
    jnz fail_map1
    mov ax,[page_frame]
    mov es,ax
    mov word ptr es:[0],5678h

    mov ah,44h
    mov al,0
    xor bx,bx
    mov dx,[ems_handle]
    int 67h
    or ah,ah
    jnz fail_remap0
    mov ax,[page_frame]
    mov es,ax
    cmp word ptr es:[0],1234h
    jne fail_content0

    mov ah,44h
    mov al,0
    mov bx,1
    mov dx,[ems_handle]
    int 67h
    or ah,ah
    jnz fail_remap1
    mov ax,[page_frame]
    mov es,ax
    cmp word ptr es:[0],5678h
    jne fail_content1

    mov ah,45h
    mov bx,[ems_handle]
    int 67h
    or ah,ah
    jnz fail_free

    mov dx,offset pass_message
    mov ah,09h
    int 21h
    mov ax,4c00h
    int 21h

fail_detect: mov dx,offset detect_message
    jmp print_exit
fail_frame: mov dx,offset frame_message
    jmp print_exit
fail_alloc:
    mov [ems_error],ah
    mov dx,offset alloc_message
    mov ah,09h
    int 21h
    mov al,[ems_error]
    mov cl,4
    shr al,cl
    call print_hex
    mov al,[ems_error]
    and al,0fh
    call print_hex
    mov dx,offset crlf
    mov ah,09h
    int 21h
    mov ax,4c01h
    int 21h
fail_map0: mov dx,offset map0_message
    jmp release_print
fail_map1: mov dx,offset map1_message
    jmp release_print
fail_remap0: mov dx,offset remap0_message
    jmp release_print
fail_remap1: mov dx,offset remap1_message
    jmp release_print
fail_content0: mov dx,offset content0_message
    jmp release_print
fail_content1: mov dx,offset content1_message
    jmp release_print
fail_free: mov dx,offset free_message
    jmp print_exit

release_print:
    mov [fail_pointer],dx
    mov ah,45h
    mov bx,[ems_handle]
    int 67h
    mov dx,[fail_pointer]
print_exit:
    mov ah,09h
    int 21h
    mov ax,4c01h
    int 21h

print_hex proc near
    and al,0fh
    add al,'0'
    cmp al,'9'
    jbe hex_digit
    add al,7
hex_digit:
    mov dl,al
    mov ah,02h
    int 21h
    ret
print_hex endp

page_frame dw 0
ems_handle dw 0
fail_pointer dw 0
ems_error db 0
pass_message db 'EMS map/read/write/free OK',13,10,'$'
detect_message db 'EMS manager not installed',13,10,'$'
frame_message db 'EMS page frame query failed',13,10,'$'
alloc_message db 'EMS page allocation failed AH=$'
crlf db 13,10,'$'
map0_message db 'EMS initial page mapping failed',13,10,'$'
map1_message db 'EMS second page mapping failed',13,10,'$'
remap0_message db 'EMS first page remapping failed',13,10,'$'
remap1_message db 'EMS second page remapping failed',13,10,'$'
content0_message db 'EMS first page data mismatch',13,10,'$'
content1_message db 'EMS second page data mismatch',13,10,'$'
free_message db 'EMS handle release failed',13,10,'$'

code ends
end start
