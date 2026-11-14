# Practical 32-bit applications

ABI 0102h is opt-in. ABI 0100h/0101h packages and addresses retain their
contracts. The outer 96-byte header is unchanged. Immediately before the ELF,
0102h requires the 24-byte little-endian `K32RES01` descriptor: u16 version=1,
size=24, u32 stack bytes, minimum heap bytes, reserved=0. Stack sizes are
page-aligned 4–256 KiB. The loader rejects insufficient space before allocation.

The physical and user budget remains 4 MiB. For 0102h, stack top is BFD000h,
the startup page remains read-only at BFD000h, and the page immediately below
the selected stack is unmapped. ELF segments and heap end before that guard.
Code is read-only; the 386 has no NX. The startup page's six former reserved
dwords expose stack base, stack bytes, guard address, requested minimum heap,
resource version=1 and reserved=0. Legacy reserved fields remain zero.

`PRACT32.EXE` requests 64 KiB stack and at least 1 MiB heap, executes a real
32 KiB stack workload and zero/fill/reallocation checks, then prints its result.
`PRACT32 hold` waits; `PRACT32 fault` deliberately writes its stack guard.
Normal exit, End and faults use the existing owner-specific reclamation.

Link `sdk/libc.c` for the explicitly prefixed memory/string, bounded format
(`%s %c %d %u %x %X %%`) and allocation subset in `sdk/libc.h`. It is not a
complete hosted C library. Allocations use the existing 64 owned allocation
slots and page-rounded backing; failed realloc retains the old allocation.
The template in `sdk/template` builds an independent application using GCC,
binutils and Python. No FPU, dynamic linking, threads or Win32 ABI is required.

Package with `--abi 258 --stack-bytes 65536 --min-heap 1048576 --debug-map
APP.debug.json`. Keep the unstripped ELF and map as build artifacts. Resolve
a recorded fault using:

```
python3 diagnose.py build/PRACT32.debug.json --elf build/practical.ext.elf \
  --exe build/PRACT32.EXE --eip 0x800000 --cr2 0xbec000
```

Both file hashes must match the map before `addr2line` runs. Actual CPU EIP/CR2
remain authoritative; this does not invent a stack trace. Internal real-mode C
uses size optimization and register arguments; all assembly-to-C entries take
no arguments. Protected application calls/CRT keep their cdecl ABI. Resident
snapshot and real/protected stack linker guards remain enforced.
