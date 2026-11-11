/* Kept within the guest-produced first-512-byte PSP snapshot. */
.section .start,"ax"
.global v86_hdd_stats
.balign 4
v86_hdd_stats:
.ascii "KVHDD001"
.long 1,64
.space 48,0
.section .note.GNU-stack,"",@progbits
