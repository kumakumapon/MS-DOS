.intel_syntax noprefix
.code16gcc
.section .header,"awx"
.long 0xffffffff
.word 0x2000
.word strategy,interrupt
.byte 1
.space 7
.section .text
.global strategy,interrupt,khd_bios,khd_sense,khd_copy
.extern khd_dispatch,khd_request,khd_bounce
strategy:
 push eax
 push edx
 xor eax,eax
 mov ax,es
 shl eax,4
 movzx edx,bx
 add eax,edx
 mov DWORD PTR cs:khd_request,eax
 pop edx
 pop eax
 retfw
interrupt:
 pushf
 pushad
 pushw ds
 pushw es
 cld
 mov WORD PTR cs:caller_ss,ss
 mov DWORD PTR cs:caller_sp,esp
 cli
 mov ax,cs
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov esp,OFFSET stack_top
 sti
 call khd_dispatch
 cli
 mov ss,WORD PTR cs:caller_ss
 mov esp,DWORD PTR cs:caller_sp
 popw es
 popw ds
 popad
 popf
 retfw
khd_bios:
 pushad
 pushw ds
 pushw es
 mov eax,DWORD PTR ss:[esp+40]
 mov edx,DWORD PTR ss:[esp+44]
 mov ecx,edx
 shr edx,16
 mov ebx,DWORD PTR ss:[esp+48]
 mov es,WORD PTR cs:khd_bounce
 xor ebp,ebp
 int 0x1b
 mov BYTE PTR cs:bios_result,ah
 popw es
 popw ds
 popad
 movzx eax,BYTE PTR cs:bios_result
 ret
khd_sense:
 pushad
 pushw ds
 pushw es
 mov ax,0x8480
 xor ebx,ebx
 xor ecx,ecx
 xor edx,edx
 int 0x1b
 jc sense_bad
 cmp bx,256
 jne sense_sasi
 cmp cx,615
 jne sense_bad
 cmp dx,0x0821
 jne sense_bad
 jmp sense_ok
sense_sasi:
 cmp ah,6
 jne sense_bad
 or ebx,ecx
 or ebx,edx
 jnz sense_bad
sense_ok:
 mov BYTE PTR cs:bios_result,0
 jmp sense_done
sense_bad:
 mov BYTE PTR cs:bios_result,1
sense_done:
 popw es
 popw ds
 popad
 movzx eax,BYTE PTR cs:bios_result
 ret
# Copy linear conventional addresses using normalized far pointers: no offset
# wraps, even when the original caller supplied e.g. segment:FF80.
khd_copy:
 pushad
 pushw ds
 pushw es
 mov eax,DWORD PTR ss:[esp+40]
 mov esi,DWORD PTR ss:[esp+44]
 mov ecx,DWORD PTR ss:[esp+48]
 mov edi,eax
 and edi,15
 shr eax,4
 mov es,ax
 mov eax,esi
 and esi,15
 shr eax,4
 mov ds,ax
 cld
 rep movsb
 popw es
 popw ds
 popad
 ret
.section .data
caller_ss:.word 0
caller_sp:.long 0
bios_result:.byte 0
.section .bss
.balign 16
.space 2048
stack_top:
.section .note.GNU-stack,"",@progbits
