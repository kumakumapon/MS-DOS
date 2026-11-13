# KXMS32: optional PC-98 high-memory supplement

Original MIT extension for the KGUI32 32 MiB profile. The vendored GPL FDXMS286
binary and sources remain unchanged; this separate driver delegates its low
handles, HMA and A20 operations to the already installed XMS 2 manager.

Load `DEVICE=KXHIGH.SYS`, then FDXMS286, then `DEVICE=KXMS32.SYS`, without EMM386. KXHIGH captures and reserves the BIOS high pool before FDXMS286 clears word 0594h. BIOS word
0594h must advertise at least 16 MiB above 16 MiB. Only then is the independent
physical 01000000h–01FFFFFFh pool installed. Lower configurations retain the old
entry and only a small inert DOS device prefix. The reserved PC-98 15–16 MiB
gap is excluded. Duplicate installation is refused through INT 2Fh/D38Ch.

The high pool has 64 handles FF00h–FF3Fh, lock counts, bounds, first-fit holes,
coalescing by scan, and in-place resize. XMS 2 functions 08h–0Fh combine free
space or route high handles; all other calls retain the original provider.
Requests are serialized. Moves resolve/lock low handles, reject overflow and
overlap, pair the original A20 reference, and copy in <=32 KiB protected-mode
bands. No DOS or BIOS runs with PE set. Each band restores the full CR0/GDTR,
16-bit code/data descriptors, segment values and incoming flags. The caller's
stack is separate from the resident 2 KiB C stack. XMS 3 is not advertised.

D38Ch returns AX=38CDh, ES:DI -> KX32MEM1 (version 1, 64 bytes), with busy/ready,
free/largest pool KiB, allocation/free/move counters, bytes and physical bounds.
It is a read-only privileged diagnostic producer, not an application interface.

`make all test` runs the allocator/range/lock/failure checks under ASan/UBSan.
The webnp2 32 MiB suite checks actual code/data transfers into high-memory
private PM32 pages, four independent contexts, and complete XMS reclamation.
