/* Original built-in PM32 payload; flat pointers, no DOS/XMS transfer API. */
.code32
user_code:
    mov eax,DWORD PTR [UM]
    cmp al,'F'
    je user_pf
    cmp al,'C'
    je user_code_write
    cmp al,'K'
    je user_kernel
    cmp al,'S'
    je user_stack
    cmp al,'P'
    je user_gp
    cmp al,'B'
    je user_spin
    cmp al,'T'
    je user_spin
    mov ebx,0x12345678
    mov ebp,0x87654321
    xor eax,eax
    mov edi,UD
    mov ecx,0x80000
    repe scasd
    jne user_fail
    mov DWORD PTR [UM+4],1
    mov edi,UD
    xor eax,eax
    mov ecx,0x80000
fill:
    mov edx,eax
    xor edx,0x5a93c6ef
    mov DWORD PTR [edi+eax*4],edx
    inc eax
    loop fill
    mov DWORD PTR [UM+4],2
    xor eax,eax
    xor esi,esi
    mov ecx,0x80000
verify:
    mov edx,DWORD PTR [edi+eax*4]
    add esi,edx
    xor edx,0x5a93c6ef
    cmp edx,eax
    jne user_fail
    inc eax
    loop verify
    cmp ebx,0x12345678
    jne user_fail
    cmp ebp,0x87654321
    jne user_fail
    mov DWORD PTR [UM+8],esi
    mov DWORD PTR [UM+4],3
    xor eax,eax
    int 0x30
user_fail:
    mov eax,1
    int 0x30
user_spin:
    inc DWORD PTR [UM+12]
    jmp user_spin
user_pf:
    mov eax,DWORD PTR [GUARD]
    jmp user_fail
user_code_write:
    mov DWORD PTR [UC],0
    jmp user_fail
user_kernel:
    mov eax,DWORD PTR [UM+16]
    mov eax,DWORD PTR [eax]
    jmp user_fail
user_stack:
    mov esp,US
    push eax
    jmp user_fail
user_gp:
    cli
    jmp user_fail
user_end:
