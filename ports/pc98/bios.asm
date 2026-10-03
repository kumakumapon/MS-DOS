; PC-98 OEM BIOS for Microsoft DOS; DOS4 selects the v4.0 SYSINIT contract.
; All hardware access uses NEC BIOS or PC-98 text VRAM, never IBM PC BIOS.
.8086
EXTRN SYSINIT:FAR, CURRENT_DOS_LOCATION:WORD, FINAL_DOS_LOCATION:WORD
EXTRN DEVICE_LIST:DWORD, MEMORY_SIZE:WORD, DEFAULT_DRIVE:BYTE
PUBLIC RE_INIT
CODE SEGMENT PARA PUBLIC 'CODE'
ASSUME CS:CODE, DS:CODE
ORG 0
start: jmp initialize
condev dw auxdev, 0
       dw 8003h, strategy, con_interrupt
       db 'CON     '
auxdev dw prndev, 0
       dw 8000h, strategy, aux_interrupt
       db 'AUX     '
prndev dw clockdev, 0
       dw 8000h, strategy, aux_interrupt
       db 'PRN     '
clockdev dw diskdev, 0
       dw 8008h, strategy, clock_interrupt
       db 'CLOCK$  '
diskdev dw -1,-1
       dw 2000h, strategy, disk_interrupt
       db 1,7 dup (0)
request dd 0
cursor dw 0
bootdrive db 90h
ansi_state db 0
ansi_row dw 0
ansi_col dw 0
text_attr dw 0e1h
ansi_colors db 0,2,4,6,1,3,5,7
bpb dw 1024
    db 1
    dw 1
    db 2
    dw 192,1232
    db 0feh
    dw 2
IFDEF DOS4
    dw 8,2
    dd 0,0
    db 6 dup(0)
    PUBLIC MulTrk_flag, KEYRD_Func, KEYSTS_Func, EC35_Flag
MulTrk_flag dw 1 ; disable IBM multi-track optimization
KEYRD_Func db 0
KEYSTS_Func db 1
EC35_Flag db 0
ENDIF
bpblist dw bpb
clockdata dw 0
          db 0,0,0,0
strategy PROC FAR
    mov WORD PTR cs:request,bx
    mov WORD PTR cs:request+2,es
    ret
strategy ENDP
con_interrupt PROC FAR
    push ax
    mov al,0
    jmp dispatch
con_interrupt ENDP
aux_interrupt PROC FAR
    push ax
    mov al,1
    jmp dispatch
aux_interrupt ENDP
clock_interrupt PROC FAR
    push ax
    mov al,2
    jmp dispatch
clock_interrupt ENDP
disk_interrupt PROC FAR
    push ax
    mov al,3
    jmp dispatch
disk_interrupt ENDP
dispatch:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    cld
    lds bx,cs:request
    mov WORD PTR [bx+3],100h
    mov ah,[bx+2]
    push cs
    pop es
    cmp al,3
    je disk_dispatch
    cmp al,2
    je clock_dispatch
    cmp al,1
    je aux_dispatch
    cmp ah,4
    je con_read
    cmp ah,5
    je con_peek
    cmp ah,7
    je con_flush
    cmp ah,8
    je con_write
    cmp ah,9
    je con_write
    jmp done
aux_dispatch:
    cmp ah,0
    je done
    jmp badcommand
con_read:
    mov cx,[bx+18]
    les di,[bx+14]
    or cx,cx
    jz done
con_read_loop:
    xor ah,ah
    int 18h
    stosb
    loop con_read_loop
    jmp done
con_peek:
    mov ah,1
    int 18h
    or bh,bh
    jz busy
    lds bx,cs:request
    mov [bx+13],al
    jmp done
con_flush:
    mov ah,1
    int 18h
    or bh,bh
    jz done
    xor ah,ah
    int 18h
    jmp con_flush
con_write:
    mov cx,[bx+18]
    lds si,[bx+14]
    or cx,cx
    jz done
con_write_loop:
    lodsb
    call putchar
    loop con_write_loop
    jmp done
clock_dispatch:
    cmp ah,4
    je clock_read
    cmp ah,8
    je clock_write
    cmp ah,9
    je clock_write
    jmp done
clock_read:
    call rtc_read
    mov cx,[bx+18]
    cmp cx,6
    jbe clock_read_size
    mov cx,6
clock_read_size:
    les di,[bx+14]
    push cs
    pop ds
    mov si,OFFSET clockdata
    rep movsb
    jmp done
clock_write:
    mov cx,[bx+18]
    cmp cx,6
    jbe clock_write_size
    mov cx,6
clock_write_size:
    lds si,[bx+14]
    mov di,OFFSET clockdata
    rep movsb
    call rtc_write
    jmp done
disk_dispatch:
    cmp BYTE PTR [bx+1],0
    jne badunit
    cmp ah,0
    je disk_init
    cmp ah,1
    je disk_media
    cmp ah,2
    je disk_bpb
    cmp ah,4
    je disk_read
    cmp ah,8
    je disk_write
    cmp ah,9
    je disk_write
    cmp ah,10
    je done
    cmp ah,11
    je done
    jmp badcommand
disk_init:
    mov BYTE PTR [bx+13],1
    mov WORD PTR [bx+14],OFFSET resident_end
    mov ax,cs
    mov [bx+16],ax
    mov WORD PTR [bx+18],OFFSET bpblist
    mov [bx+20],ax
    jmp done
disk_media:
    mov BYTE PTR [bx+14],0 ; unknown: force DOS to reread FAT after swaps
    jmp done
disk_bpb:
    mov WORD PTR [bx+18],OFFSET bpb
    mov ax,cs
    mov [bx+20],ax
    jmp done
disk_read:
    mov si,56h
    jmp disk_transfer
disk_write:
    mov si,55h
disk_transfer:
    mov di,[bx+20] ; logical sector
    mov cx,[bx+18]
    cmp di,1232
    jae disk_error
    mov ax,di
    add ax,cx
    jc disk_error
    cmp ax,1232
    ja disk_error
    les bp,[bx+14]
    or cx,cx
    jz done
disk_sector:
    push cx
    push di
    push bp
    push es
    ; A sector-aligned resident bounce buffer never crosses a DMA 64K boundary.
    cmp si,55h
    jne disk_dma
    push ds
    push si
    push di
    push cx
    push es
    pop ds
    mov si,bp
    push cs
    pop es
    mov di,OFFSET bounce
    mov cx,512
    rep movsw
    pop cx
    pop di
    pop si
    pop ds
disk_dma:
    push cs
    pop es
    mov bp,OFFSET bounce
    mov ax,di
    xor dx,dx
    mov bx,16
    div bx
    mov cl,al
    mov ch,3 ; 1024-byte physical sector
    mov ax,dx
    xor dx,dx
    mov bx,8
    div bx
    mov dh,al ; head
    inc dl    ; 1-based sector
    mov ax,si
    mov ah,al
    mov al,cs:bootdrive
    mov bx,1024
    push si
    push ds
    int 1bh
    pop ds
    pop si
    pop es
    pop bp
    pop di
    pop cx
    jc disk_error
    cmp si,56h
    jne disk_advance
    push ds
    push si
    push di
    push cx
    push cs
    pop ds
    mov si,OFFSET bounce
    mov di,bp
    mov cx,512
    rep movsw
    pop cx
    pop di
    pop si
    pop ds
disk_advance:
    inc di
    add bp,1024
    jnc disk_no_wrap
    mov ax,es
    add ax,1000h
    mov es,ax
disk_no_wrap:
    loop disk_sector
    jmp done
disk_error:
    lds bx,cs:request
    sub di,[bx+20]
    mov [bx+18],di
    mov ax,810bh
    jmp status_exit
badunit:
    mov ax,8101h
    jmp status_exit
badcommand:
    mov ax,8103h
    jmp status_exit
busy:
    mov ax,300h
status_exit:
    lds bx,cs:request
    mov [bx+3],ax
done:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    retf

; PC-98 RTC: six-byte calendar through INT 1Ch, AH=0/1, ES:BX.
; DOS packet: days since 1980, minute, hour, hundredth, second.
rtctime db 6 dup(0)
month_days db 31,28,31,30,31,30,31,31,30,31,30,31
rtc_read PROC NEAR
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push cs
    pop ds
    push cs
    pop es
    mov bx,OFFSET rtctime
    xor ah,ah
    int 1ch
    mov al,rtctime
    call from_bcd
    cmp ax,80
    jae rtc_1900
    add ax,100
rtc_1900:
    sub ax,80
    mov di,ax
    mov cx,ax
    xor si,si
    xor dx,dx
    jcxz rtc_month
rtc_year:
    add si,365
    test dl,3
    jnz rtc_notleap
    inc si
rtc_notleap:
    inc dx
    loop rtc_year
rtc_month:
    mov al,rtctime+1
    mov cl,4
    shr al,cl
    xor ah,ah
    cmp ax,1
    jb rtc_invalid
    cmp ax,12
    ja rtc_invalid
    dec ax
    mov cx,ax
    xor bx,bx
    jcxz rtc_day
rtc_addmonth:
    xor ah,ah
    mov al,month_days[bx]
    add si,ax
    cmp bx,1
    jne rtc_nextmonth
    test di,3
    jnz rtc_nextmonth
    inc si
rtc_nextmonth:
    inc bx
    loop rtc_addmonth
rtc_day:
    mov al,rtctime+2
    call from_bcd
    cmp ax,1
    jb rtc_invalid
    cmp ax,31
    ja rtc_invalid
    dec ax
    add si,ax
    mov clockdata,si
    mov al,rtctime+4
    call from_bcd
    mov BYTE PTR clockdata+2,al
    mov al,rtctime+3
    call from_bcd
    mov BYTE PTR clockdata+3,al
    mov BYTE PTR clockdata+4,0
    mov al,rtctime+5
    call from_bcd
    mov BYTE PTR clockdata+5,al
rtc_invalid:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
rtc_read ENDP
rtc_write PROC NEAR
    push ax
    push bx
    push cx
    push dx
    push si
    push ds
    push es
    push cs
    pop ds
    push cs
    pop es
    mov si,clockdata
    xor bx,bx
rtc_findyear:
    mov ax,365
    test bl,3
    jnz rtc_yearsize
    inc ax
rtc_yearsize:
    cmp si,ax
    jb rtc_foundyear
    sub si,ax
    inc bx
    jmp rtc_findyear
rtc_foundyear:
    mov ax,bx
    add ax,80
    cmp ax,100
    jb rtc_year_bcd
    sub ax,100
rtc_year_bcd:
    call to_bcd
    mov rtctime,al
    xor cx,cx
rtc_findmonth:
    mov dx,bx
    mov bx,cx
    xor ah,ah
    mov al,month_days[bx]
    mov bx,dx
    cmp cx,1
    jne rtc_monthsize
    test bl,3
    jnz rtc_monthsize
    inc ax
rtc_monthsize:
    cmp si,ax
    jb rtc_foundmonth
    sub si,ax
    inc cx
    cmp cx,12
    jb rtc_findmonth
    jmp rtc_write_exit
rtc_foundmonth:
    inc cx
    mov al,cl
    mov cl,4
    shl al,cl
    mov rtctime+1,al
    mov ax,si
    inc ax
    call to_bcd
    mov rtctime+2,al
    mov al,BYTE PTR clockdata+3
    call to_bcd
    mov rtctime+3,al
    mov al,BYTE PTR clockdata+2
    call to_bcd
    mov rtctime+4,al
    mov al,BYTE PTR clockdata+5
    call to_bcd
    mov rtctime+5,al
    mov bx,OFFSET rtctime
    mov ah,1
    int 1ch
rtc_write_exit:
    pop es
    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
rtc_write ENDP
from_bcd PROC NEAR
    xor ah,ah
    push bx
    mov bl,al
    and bl,15
    mov cl,4
    shr al,cl
    mov ah,10
    mul ah
    xor bh,bh
    add ax,bx
    pop bx
    ret
from_bcd ENDP
to_bcd PROC NEAR
    push bx
    xor ah,ah
    mov bl,10
    div bl
    mov bl,ah
    mov cl,4
    shl al,cl
    or al,bl
    pop bx
    ret
to_bcd ENDP

; Preserve every register; DOS has a small driver stack.
putchar PROC NEAR
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push cs
    pop ds
    mov bx,cursor
    cmp ansi_state,0
    jne ansi_input
    cmp al,27
    jne normal_output
    mov ansi_state,1
    jmp output_end
normal_output:
    cmp al,13
    je output_cr
    cmp al,10
    je output_lf
    cmp al,8
    je output_bs
    cmp al,9
    je output_tab
    cmp al,32
    jb output_end
    mov dx,0a000h
    mov es,dx
    mov di,bx
    xor ah,ah
    stosw
    mov ax,text_attr
    mov es:[bx+2000h],ax
    add bx,2
    jmp output_scroll
ansi_input:
    cmp ansi_state,1
    jne ansi_parameters
    cmp al,'['
    jne ansi_cancel
    mov ansi_state,2
    mov ansi_row,0
    mov ansi_col,0
    jmp output_end
ansi_parameters:
    cmp al,'0'
    jb ansi_nondigit
    cmp al,'9'
    ja ansi_nondigit
    sub al,'0'
    xor ah,ah
    mov si,OFFSET ansi_row
    cmp ansi_state,3
    jne ansi_digit
    mov si,OFFSET ansi_col
ansi_digit:
    mov dx,[si]
    cmp dx,100
    ja ansi_cancel
    mov cx,ax
    mov ax,10
    mul dx
    add ax,cx
    mov [si],ax
    jmp output_end
ansi_nondigit:
    cmp al,';'
    jne ansi_command
    mov ansi_state,3
    jmp output_end
ansi_command:
    cmp al,'H'
    je ansi_position
    cmp al,'f'
    je ansi_position
    cmp al,'J'
    je ansi_erase
    cmp al,'m'
    je ansi_attribute
    jmp ansi_cancel
ansi_attribute:
    mov si,ansi_row
    cmp si,0
    jne ansi_setcolor
    mov text_attr,0e1h
    jmp ansi_cancel
ansi_setcolor:
    sub si,30
    cmp si,7
    ja ansi_cancel
    mov al,ansi_colors[si]
    mov cl,5
    shl al,cl
    or al,1
    xor ah,ah
    mov text_attr,ax
    jmp ansi_cancel
ansi_position:
    mov ax,ansi_row
    or ax,ax
    jz ansi_rowdefault
    dec ax
ansi_rowdefault:
    cmp ax,24
    ja ansi_cancel
    mov cx,160
    mul cx
    mov bx,ax
    mov ax,ansi_col
    or ax,ax
    jz ansi_coldefault
    dec ax
ansi_coldefault:
    cmp ax,79
    ja ansi_cancel
    add ax,ax
    add bx,ax
    jmp ansi_cancel
ansi_erase:
    cmp ansi_row,2
    jne ansi_cancel
    mov ax,0a000h
    mov es,ax
    xor di,di
    mov ax,32
    mov cx,2000
    rep stosw
    mov di,2000h
    mov ax,text_attr
    mov cx,2000
    rep stosw
    xor bx,bx
ansi_cancel:
    mov ansi_state,0
    jmp output_end
output_cr:
    mov ax,bx
    xor dx,dx
    mov cx,160
    div cx
    sub bx,dx
    jmp output_end
output_lf:
    add bx,160
    jmp output_scroll
output_bs:
    or bx,bx
    jz output_end
    sub bx,2
    jmp output_end
output_tab:
    add bx,16
    and bx,0fff0h
output_scroll:
    cmp bx,4000
    jb output_end
    mov ax,0a000h
    mov ds,ax
    mov es,ax
    mov si,160
    xor di,di
    mov cx,1920
    rep movsw
    mov ax,32
    mov cx,80
    rep stosw
    push cs
    pop ds
    sub bx,160
output_end:
    mov cursor,bx
    mov dx,bx
    or dx,1
    mov ah,13h
    int 18h
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
putchar ENDP
fast_output PROC FAR
    pushf
    call putchar
    popf
    iret
fast_output ENDP
RE_INIT PROC FAR
    ret
RE_INIT ENDP
    db (1024 - (($-start) MOD 1024)) MOD 1024 dup(0)
bounce db 1024 dup(0)
resident_end LABEL BYTE
initialize:
    cld
    cli
    mov ax,cs
    mov ss,ax
    mov sp,OFFSET init_stack_end
    mov ds,ax
    sti
    xor ax,ax
    mov es,ax
    mov al,es:[584h]
    mov bootdrive,al
    ; SYSINIT creates INT 21h; provide fast console INT 29h here.
    mov WORD PTR es:[29h*4],OFFSET fast_output
    mov WORD PTR es:[29h*4+2],cs
    mov si,OFFSET condev
init_links:
    mov WORD PTR [si+2],cs
    mov si,[si]
    cmp si,OFFSET diskdev
    jne init_links
    mov ah,0ah
    xor al,al
    int 18h
    mov ah,0ch
    int 18h
    mov ah,11h
    int 18h
    mov ax,0a000h
    mov es,ax
    xor di,di
    mov cx,2000
    mov ax,32
    rep stosw
    mov di,2000h
    mov cx,2000
    mov ax,0e1h
    rep stosw
    mov ax,SEG SYSINIT
    mov ds,ax
ASSUME DS:SEG SYSINIT
    mov WORD PTR CURRENT_DOS_LOCATION,3000h
    mov ax,1000h
    mov FINAL_DOS_LOCATION,ax
    mov WORD PTR DEVICE_LIST,OFFSET condev
    mov WORD PTR DEVICE_LIST+2,cs
    mov WORD PTR MEMORY_SIZE,0a000h
IFDEF DOS4
    mov BYTE PTR DEFAULT_DRIVE,1 ; DOS 4 SYSINIT uses one-based A:
ELSE
    mov BYTE PTR DEFAULT_DRIVE,0
ENDIF
    jmp SYSINIT
    db 256 dup (0)
init_stack_end LABEL BYTE
CODE ENDS
END start
