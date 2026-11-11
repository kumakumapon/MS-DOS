/* Bounded FAT16 system loader for the marked 1024-byte-sector profile only. */
.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
 jmp entry
 nop
 .ascii "KHDS2V1!"
entry:
 cld
 mov ax,cs
 mov ds,ax
 mov es,ax
 /* Partition table and BPB are checked before trusting file locations. */
 mov bp,OFFSET scratch
 mov cx,1
 xor dx,dx
 mov bx,512
 call read
 jc failed
 mov si,OFFSET partition
 mov di,OFFSET scratch
 mov cx,32
 repe cmpsb
 jne failed
 mov cx,480
 xor al,al
 repe scasb
 jne failed
 mov bp,OFFSET scratch
 mov cx,264
 mov bx,1024
 call read
 jc failed
 mov si,OFFSET bpb_prefix
 mov di,OFFSET scratch+3
 mov cx,33
 repe cmpsb
 jne failed
 cmp WORD PTR scratch+510,0xaa55
 jne failed
 mov si,OFFSET label
 mov di,OFFSET scratch+43
 mov cx,19
 repe cmpsb
 jne failed
 /* Read fixed root and FAT into nonoverlapping, DMA-safe temporary regions. */
 mov ax,0x6000
 mov es,ax
 xor bp,bp
 mov cx,428
 mov bx,16384
 call read
 jc failed
 mov ax,0x6800
 mov es,ax
 mov cx,268
 mov bx,20480
 call read
 jc failed
 mov si,OFFSET io_name
 mov di,OFFSET descriptor+12
 mov ax,0xf400
 call system_file
 mov si,OFFSET dos_name
 mov di,OFFSET descriptor+22
 mov ax,0xa000
 call system_file
 mov si,WORD PTR descriptor+12
 mov di,WORD PTR descriptor+14
 mov ax,0x00c0
 mov es,ax
 xor bp,bp
 call load
 mov si,WORD PTR descriptor+22
 mov di,WORD PTR descriptor+24
 mov ax,0x3000
 mov es,ax
 xor bp,bp
 call load
 ljmp 0x00c0,0
/* SI=name, DI=(logical start,count,size,first cluster), AX=load-size ceiling. */
system_file:
 cmp DWORD PTR [di+4],0
 je failed
 movzx eax,ax
 cmp DWORD PTR [di+4],eax
 ja failed
 mov eax,DWORD PTR [di+4]
 add eax,1023
 shr eax,10
 cmp ax,WORD PTR [di+2]
 jne failed
 mov bx,WORD PTR [di+8]
 cmp bx,2
 jb failed
 cmp bx,10117
 ja failed
 mov ax,bx
 sub ax,2
 shl ax,2
 add ax,57
 cmp ax,WORD PTR [di]
 jne failed
 /* Find the exact regular system file and bind descriptor to its root entry. */
 push di
 mov ax,0x6000
 mov es,ax
 xor bx,bx
 mov dx,512
search:
 cmp BYTE PTR es:[bx],0
 je failed
 push si
 mov di,bx
 mov cx,11
 repe cmpsb
 pop si
 je found
 add bx,32
 dec dx
 jnz search
 jmp failed
found:
 pop di
 cmp BYTE PTR es:[bx+11],6
 jne failed
 mov eax,DWORD PTR es:[bx+28]
 cmp eax,DWORD PTR [di+4]
 jne failed
 mov bx,WORD PTR es:[bx+26]
 cmp bx,WORD PTR [di+8]
 jne failed
 /* Only exact contiguous allocation is accepted; never follow an arbitrary LBA. */
 mov ax,0x6800
 mov es,ax
 mov eax,DWORD PTR [di+4]
 add eax,4095
 shr eax,12
 mov cx,ax
chain:
 cmp bx,10117
 ja failed
 mov bp,bx
 shl bp,1
 mov ax,WORD PTR es:[bp]
 dec cx
 jz last
 inc bx
 cmp ax,bx
 jne failed
 jmp chain
last:
 cmp ax,0xfff8
 jb failed
 ret
load:
 movzx ecx,si
 shl ecx,2
 add ecx,264
 mov edx,ecx
 shr edx,16
 and ecx,65535
 mov bx,1024
 call read
 jc failed
 add bp,1024
 inc si
 dec di
 jnz load
 ret
read:
 pushad
 push ds
 push es
 mov ax,0x0600
 int 0x1b
 pop es
 pop ds
 popad
 ret
failed:
 mov ax,cs
 mov ds,ax
 mov ax,0xa000
 mov es,ax
 xor di,di
 mov si,OFFSET message
 mov cx,14
1: lodsb
 xor ah,ah
 stosw
 loop 1b
 mov ax,0xa200
 mov es,ax
 xor di,di
 mov ax,0x00e1
 mov cx,14
 rep stosw
 cli
2: hlt
 jmp 2b
message: .ascii "HDD BOOT ERROR"
partition:
 .byte 0xa1,0x91,0,0,0,0,1,0,0,0,1,0,0,0,0x66,2
 .ascii "KHDPROOF        "
bpb_prefix:
 .ascii "KHDPRF01"
 .word 1024
 .byte 4
 .word 1
 .byte 2
 .word 512,40524
 .byte 0xf8
 .word 20,33,8
 .long 66,0
label: .ascii "KHDPROOF   FAT16   "
io_name: .ascii "IO      SYS"
dos_name: .ascii "MSDOS   SYS"
/* Patched only by the offline generator, bounded again before any payload read. */
descriptor:
 .ascii "KHDBOOT1"
 .word 1,32
 .word 0,0
 .long 0
 .word 0
 .word 0,0
 .long 0
 .word 0
scratch: .space 1024
.org 4095
.byte 0
