# K38632 external EXE loader and minimal C SDK

`K32RUN.EXE` loads a separately built original PM32 application. It uses the
same protected execution, XMS ownership, IRQ0/context restoration and cleanup
engine as K32MEM. This milestone has one PM32 owner; mixed PM32/V86 scheduling,
PM32 GUI windows remain later work. ABI 0101h adds startup arguments, an owned
heap, queued input/waits and read-only files; see [SERVICES.md](SERVICES.md).

## Build and use

```
make -j4 -C tools/k386 all test
# SDK applications alone:
make -C tools/k38632 apps
```

GNU GCC/binutils and Python are sufficient. The SDK compiles freestanding C
with `-m32 -march=i386`, soft float, no PIE, no stack protector and no libc.
`sdk/crt0.S`, `sdk/app.ld`, `sdk/k32.h` and `package.py` define the build contract.
The examples show the Makefile rules for a new application. `main(void)` returns
an application status. Constructors, TLS, dynamic linking, FPU/SIMD and the C
standard library are unsupported.

Use the experimental WebNP2 disk at 16 MiB, FDXMS286 and no EMM386. Save work and
F10 out of KDESK to real DOS, then run:

```
K32RUN HELLO32.EXE
K32RUN MEM32.EXE
K32RUN BADPTR32.EXE
K32RUN FAULT32.EXE
K32RUN /T SPIN32.EXE
K32RUN SPIN32.EXE
```

The last command runs until Escape or the default dispatch budget. `/T` selects
64 dispatches. A dispatch is a returned IRQ0 or system call, so a loop issuing
system calls also consumes the budget. HELLO32 prints text, calls yield/ticks,
then returns status 42. MEM32 verifies a >64 KiB file (including reads across its
64 KiB boundary), >8 KiB executed code, and zero/write/read/checksum of 2 MiB BSS.
BADPTR32 verifies API errors; FAULT32 deliberately reads supervisor memory.

DONE means reclamation succeeded; inspect the application status or fault reason
for its outcome. The 32-bit exit value is recorded and its low byte returned to
DOS. Fault/forced termination returns 255, rejected files return 2, runner or
memory failure returns 1. `KDESK` restarts the desktop. PE/V86 ownership is refused
before loading; these EXEs do not yet run as KDESK PM32 windows.

## Container and validation

The `.EXE` is a DOS MZ stub with an appended static ELF32. MZ load-size fields
cover only the short DOS stub, preventing the ELF from being loaded into
conventional RAM by DOS. Direct invocation prints `Use K32RUN APP32.EXE` and
returns 1. K32RUN reads the complete file size independently and streams payload
segments through a 4 KiB conventional buffer into its owned XMS block.

The 96-byte container header has a `K386APP1`, ABI 0100h, **kind 3** descriptor at
offset 28. Existing V86/desktop classifiers reject this unsupported kind instead
of executing it as a V86 application. At offset 72, a 24-byte descriptor contains
`K32EXE01`, u16 ABI=0100h, u16 size=24, u32 ELF offset, u32 ELF length, u32 reserved=0.
MZ relocations are empty, header size is six paragraphs and its relocation offset
is 96. ELF offset must follow the stub and be at most 4096; ELF length must exactly
cover the remaining file and be at most 4 MiB.

`elf.c` is a pure bounded read-callback parser shared by guest and host tests.
It accepts little-endian i386 ELF32 ET_EXEC, version 1, System V ABI 0, with up to
eight program headers. Only PT_NULL/PT_LOAD are supported. PT_LOAD must have
nonzero memory size, file size <= memory size, 4096-byte alignment of address and
file offset, and declared read permission. Declared writable/executable segments
are rejected. Entry must be within file-backed executable bytes. File/address
arithmetic and page-rounded virtual overlap are checked before allocation.
Overlapping nonempty payload file ranges are also rejected.

Loading is based on **program headers**, not section headers; section tables
can be absent. Static ELF physical-address fields are not allocation authority.
PT_INTERP/PT_DYNAMIC/PT_TLS and other unsupported program header types are rejected.
The reader checks seek positions, exact reads and bounds; failed validation or
loading closes the file and unwinds owned resources. Files open read-only, and
the image handle is closed before executing the payload.

## Address space and ownership

The current physical budget remains 4 MiB plus a 4 KiB alignment allowance.
Every byte is zeroed with the real XMS driver before payload publication.
ELF segments occupy page-aligned addresses in [800000h, BFD000h), with physical
offsets relative to 800000h. Code/rodata are user read-only; writable segments
and BSS are user RW. The stack is BFF000h–BFFFFFh, initial ESP=C00000h, with the
BFE000h guard unmapped. Other unused user pages are unmapped. Low kernel identity
pages remain supervisor-only. ABI 0100h exposes no heap. The 386 has no NX; execute
protection for data pages is not claimed.

Normal exit, user exceptions, budget and Escape revoke all user mappings before
unlock/free. XMS free/largest block, A20 and host CPU/descriptor/PIC state are
checked as in MEMORY.md. Insufficient XMS on a 4 MiB machine closes the image
without entering PM. Existing DOS kernel, resident V86 ABI and emulator remain
separate from this execution path.

## System call ABI 0100h

Use INT30h with EAX=function, EBX=arg0 and ECX=arg1. EAX is the result; other GPRs,
segment registers and user flags are preserved across resumption.

| EAX | Function | Arguments and result |
| --- | --- | --- |
| 0 | exit | EBX=32-bit status; does not return |
| 1 | write | EBX=linear pointer, ECX=length 0..4096; returns actual stdout bytes or negative error |
| 2 | yield | Returns 0 after returning through the real-mode scheduler |
| 3 | ticks | Returns the wrapping 24-bit PC-98 ARTIC counter (307200 ticks/sec) |

Write validates the complete nonempty range, overflow and every user-present
page before copying to a private buffer. Kernel addresses, unmapped pages and
guard crossings return -14; length >4096 returns -22. Zero length returns 0
without reading the pointer. Call 4 queries features (0 for this ABI); calls 5..12 require ABI 0101h.
Unknown calls return -38; DOS stdout failure -5.
The broker performs DOS output only after restoring real mode; stdout follows
normal DOS handle semantics. There is no direct DOS/BIOS API for the application.

## Records and tests

The record signature is `K32RUN01`, version 0100h, 1024 bytes. Common CPU, XMS,
restoration and frame offsets are described in MEMORY.md. The linked-offset and
clock-tag checks exclude uninitialized file copies. Resume count includes actual
resumption after system calls as well as IRQ0. At offset 468 the first writable
dword is sampled; 16 boundary samples are captured when its segment spans 2 MiB.

Additional u32 fields: 576 parser error, 580 file bytes, 584 entry, 588 segment
count, 592 writable start, 596 writable bytes, 604 image handle open, 608 system
calls, 612 accepted write requests, 616 yields, 620 tick calls, 624 pointer errors,
628 application status, 632 unknown calls, 636 dispatches. Offset 704 contains a
64-byte NUL-terminated filename. Rejection is state 6/error 20; allocation
refusal state 6/error 8; normal user faults retain state 4 and the real CPU error.

Host tests use actual packaged applications with the same C parser, malformed
containers/ELFs, overflow/overlap, missing section tables, V86 classifier refusal
and the runner's MZ/private-memory bounds. WebNP2 `verify-k38632-loader.mjs` uses
real DOS/XMS/CPU, actual live PTEs and post-cleanup revocation, user data samples,
system call/exit records, malformed files, fault/budget/Escape, GUI recovery,
real low-memory refusal and whole-disk SHA-256 invariance. K32CPU/K32MEM and all
existing V86/GUI/XMS/XARRAY checks continue as regression gates.
