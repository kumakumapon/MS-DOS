.code32
.section .text.wide
.global wide_code
wide_code:
 .fill 8192,1,0x90
 ret
.section .note.GNU-stack,"",@progbits
