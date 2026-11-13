.intel_syntax noprefix
.code16gcc
.section .header,"awx"
.long 0xffffffff
.word 0x8000
.word strategy,device_interrupt
.ascii "KXMS32$ "
.section .prefix,"awx"
 .global kx_request
kx_request:.long 0
init_seen:.byte 0
.global strategy
strategy:
 push eax
 push edx
 xor eax,eax
 mov ax,es
 shl eax,4
 movzx edx,bx
 add eax,edx
 mov DWORD PTR cs:kx_request,eax
 pop edx
 pop eax
 retfw
device_interrupt:
 push eax
 push ebx
 pushw es
 mov eax,DWORD PTR cs:kx_request
 mov ebx,eax
 and ebx,15
 shr eax,4
 mov es,ax
 cmp BYTE PTR es:[ebx+2],0
 jne device_status
 cmp BYTE PTR cs:init_seen,0
 jne device_status
 mov BYTE PTR cs:init_seen,1
 popw es
 pop ebx
 pop eax
 jmp device_init
 device_status:
 mov WORD PTR es:[ebx+3],0x8103
 cmp BYTE PTR es:[ebx+2],6
 je device_ready
 cmp BYTE PTR es:[ebx+2],10
 jne device_finish
 device_ready:mov WORD PTR es:[ebx+3],0x100
 device_finish:popw es
 pop ebx
 pop eax
 retfw
 device_init:
 pushfd
 pushad
 pushw ds
 pushw es
 mov WORD PTR cs:device_ss,ss
 mov DWORD PTR cs:device_sp,esp
 cli
 mov ax,cs
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov esp,OFFSET stack_top
 cld
 call kx_init
 cli
 mov ss,WORD PTR cs:device_ss
 mov esp,DWORD PTR cs:device_sp
 popw es
 popw ds
 popad
 popfd
 retfw
.global inactive_end
inactive_end:
.section .text
.global kx_api,kx_interrupt,kx_old,kx_copy
kx_interrupt:
 cmp ax,0xd38c
 je kx_query
 cmp ax,0x4310
 jne chain
 mov ax,cs
 mov es,ax
 mov bx,OFFSET kx_api
 iretw
kx_query:
 mov ax,cs
 mov es,ax
 mov di,OFFSET kx_record
 mov ax,0x38cd
 iretw
chain:
 .byte 0x2e,0xff,0x2e
 .word kx_old_vector
kx_api:
 pushfd
 cli
 cmp WORD PTR cs:kx_record+12,0
 jne busy
 mov WORD PTR cs:kx_record+12,1
 mov DWORD PTR cs:kx_regs,eax
 mov DWORD PTR cs:kx_regs+4,ebx
 mov DWORD PTR cs:kx_regs+8,edx
 mov DWORD PTR cs:kx_regs+12,esi
 mov WORD PTR cs:kx_regs+16,ds
 pushad
 pushw ds
 pushw es
 mov WORD PTR cs:api_ss,ss
 mov DWORD PTR cs:api_sp,esp
 mov eax,DWORD PTR ss:[esp+36]
 mov DWORD PTR cs:api_flags,eax
 mov ax,cs
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov esp,OFFSET stack_top
 cld
 # Interrupts may occur between bounded protected copies. Re-entry fails.
 test WORD PTR cs:api_flags,0x200
 jz api_no_sti
 sti
api_no_sti:
 call kx_dispatch
 cli
 mov WORD PTR cs:kx_record+12,0
 mov ss,WORD PTR cs:api_ss
 mov esp,DWORD PTR cs:api_sp
 popw es
 popw ds
 popad
 mov ax,WORD PTR cs:kx_regs
 mov bx,WORD PTR cs:kx_regs+4
 mov dx,WORD PTR cs:kx_regs+8
 popfd
 retfw
busy:
 popfd
 mov ax,0
 mov bl,0x8e
 retfw
# Call the original XMS entry with its original DS:SI argument.
kx_old:
 push ebx
 push esi
 pushw ds
 pushw es
 mov eax,DWORD PTR kx_call
 mov ebx,DWORD PTR kx_call+4
 mov edx,DWORD PTR kx_call+8
 mov esi,DWORD PTR kx_call+12
 mov ds,WORD PTR kx_call+16
 .byte 0x2e,0xff,0x1e
 .word kx_old_entry
 mov DWORD PTR cs:kx_call,eax
 mov DWORD PTR cs:kx_call+4,ebx
 mov DWORD PTR cs:kx_call+8,edx
 popw es
 popw ds
 pop esi
 pop ebx
 ret
# A real-mode caller supplies validated physical addresses. No DOS or BIOS in
# protected mode; copy <=32768 bytes, then restore CR0/GDTR/segments/IF exactly.
kx_copy:
 pushfd
 cli
 pushad
 pushw ds
 pushw es
 mov edi,DWORD PTR ss:[esp+44]
 mov esi,DWORD PTR ss:[esp+48]
 mov edx,DWORD PTR ss:[esp+52]
 data32 sgdt cs:saved_gdt
 mov eax,cr0
 mov DWORD PTR cs:saved_cr0,eax
 xor eax,eax
 mov ax,cs
 mov WORD PTR cs:real_target+2,ax
 shl eax,4
 mov WORD PTR cs:gdt_code+2,ax
 mov WORD PTR cs:gdt_data+2,ax
 mov ebx,eax
 shr ebx,16
 mov BYTE PTR cs:gdt_code+4,bl
 mov BYTE PTR cs:gdt_data+4,bl
 add eax,OFFSET gdt
 mov DWORD PTR cs:copy_gdt+2,eax
 data32 lgdt cs:copy_gdt
 mov eax,cr0
 or eax,1
 mov cr0,eax
 .byte 0xea
 .word copy_pm,0x10
copy_pm:
 mov ax,8
 mov ds,ax
 mov es,ax
 mov ecx,edx
 shr ecx,2
 addr32 rep movsd
 mov ecx,edx
 and ecx,3
 addr32 rep movsb
 mov ax,0x18
 mov ds,ax
 mov es,ax
 mov eax,DWORD PTR cs:saved_cr0
 mov cr0,eax
 .byte 0x2e,0xff,0x2e
 .word real_target
copy_real:
 data32 lgdt cs:saved_gdt
 popw es
 popw ds
 popad
 popfd
 ret
.section .data
real_target:.word copy_real,0
copy_gdt:.word 31
.long 0
saved_gdt:.word 0
.long 0
saved_cr0:.long 0
gdt:
.quad 0
.word 0xffff,0
.byte 0,0x92,0xcf,0
gdt_code:
.word 0xffff,0
.byte 0,0x9a,0,0
gdt_data:
.word 0xffff,0
.byte 0,0x92,0,0
device_ss:.word 0
device_sp:.long 0
api_flags:.long 0
api_ss:.word 0
api_sp:.long 0
.section .bss
.balign 16
.space 2048
stack_top:
.section .note.GNU-stack,"",@progbits
