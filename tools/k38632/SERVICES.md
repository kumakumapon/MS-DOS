# K38632 practical SDK, ABI 0101h

This extends the standalone single-owner PM32 runner. It does not add mixed
PM32/V86 scheduling or KDESK PM32 windows. Exit KDESK with F10, use the separate
experimental disk with 16 MiB and FDXMS286, and run:

```
K32RUN ARGS32.EXE "two words" three
K32RUN HEAP32.EXE
K32RUN INPUT32.EXE
K32RUN FILE32.EXE
K32RUN VIEW32.EXE VIEWDATA.TXT
```

INPUT32 expects `a` then `b`. VIEW32 supports n/space/PageDown, p/PageUp,
j/Down, k/Up, Home, End and q; Escape always stops the owner. It previews at
most 64 KiB / 1024 lines, replaces control characters, and does not edit files.
HEAP32 and FILE32 accept `hold` (Escape cleanup) or `fault` (real protection
fault cleanup). DATA32.BIN is a deterministic 70001-byte read/seek fixture.

## Build and discovery

Use `sdk/k32x.h`, `sdk/crt1.S`, `sdk/app.ld` and the freestanding GCC flags in
the Makefile. Entry is `int main(int argc, char **argv)`. Package with
`python3 package.py input.elf OUTPUT.EXE --abi 257 --features 15`.

The outer K386APP1 kind-3 classifier remains ABI 0100h. K32EXE01 uses ABI 0101h
and its last dword is the required feature mask: startup=1, heap=2, input=4,
files=8. Unknown required bits are rejected before allocation. ABI 0100h files
retain their reserved-zero header, main(void), mappings and services 0..3.
Call 4 returns available features (15 for 0101h, 0 for 0100h); services 5..12
return -38 for old applications. There is no direct DOS/BIOS access.

The read-only page BFD000h contains K32Info (64 bytes), an argv table at +64
and NUL-terminated argument strings at +128. Magic=3149324Bh, ABI=257,
size=64, argc, argv, features, heap_base, heap_limit, page_size=4096,
tick_rate=307200, then six reserved zero dwords. DOS tails are bounded to
126 bytes, at most eight arguments of 63 characters, with double-quote grouping.
argv[0] is the EXE filename; `/T` preceding it selects the existing 64-dispatch
budget. BFE000h remains an unmapped stack guard and BFF000h the stack.

## Broker interface

INT30h: EAX=number, EBX/ECX/EDX=arguments, EAX=result; other registers and user
flags are preserved. Original calls 0..3 are unchanged. Negative results are
errors. New calls run only after the real-mode CPU and host CR3 are restored.

| EAX | Service | Arguments / result |
| --- | --- | --- |
| 4 | query | feature bits |
| 5 | alloc | EBX=nonzero bytes; page-aligned pointer or error |
| 6 | free | EBX=exact owned allocation base; 0 succeeds |
| 7 | poll_key | BIOS scan high byte / ASCII low byte, or -11 when empty |
| 8 | wait | EBX=key(1), time(2), or both; ECX=0..30000 ms; result=ready bits |
| 9 | open | EBX=user filename, ECX=length 1..12; positive owned token |
| 10 | read | EBX=token, ECX=RW user buffer, EDX=0..4096 bytes; bytes read / 0 EOF |
| 11 | seek | EBX=token, ECX=absolute position through EOF; position |
| 12 | close | EBX=token; 0 succeeds |

Errors: -22 invalid request, -12 heap exhausted/64 allocation slots, -14 invalid
user range/permission, -11 empty input, -9 stale/invalid file token, -24 eight
files already open, -2 absent/nonregular file, -5 broker I/O failure, -38 unknown
service. Zero-length read does not dereference the pointer but validates ownership.

The heap uses trailing pages after the highest ELF segment, before BFD000h,
within the existing 4 MiB physical budget. Allocation is first-fit, page rounded,
zeroed through XMS before publication; there is no growth beyond that budget.
Free rejects interior, code and stale pointers, revokes all allocation PTEs and
flushes TLB by the next CR3 load. Reuse zeros again. The 386 has no NX.

Wait state 7 polls the hardware keyboard and wrapping ARTIC clock in real mode.
It does not repeatedly enter Ring 3 or consume dispatch budget while waiting.
It is not a low-power idle implementation. A private 64-event queue preserves
normal keys between syscalls/IRQs; overflow drops new keys and is counted.
Escape remains reserved even behind normal queued input. Timeouts are less than
one ARTIC wrap. Raw PC-98 keys are exposed; no IME or Unicode contract is added.

Files are read-only ASCII 8.3 regular files on A: root. No paths, drive prefixes,
wildcards, reserved DOS devices (including extensions), directories or writes.
FindFirst uses a private DTA and restores the caller DTA before returning.
Eight owned slots use generation tokens, never exposed DOS handles. Seek checks
file length, and read validates the entire writable range before DOS I/O. XMS
odd-length copies preserve neighboring bytes. Disk fixtures are never modified.
All live handles and heap mappings are reclaimed on normal exit, Escape,
budget or exception before unlocking/freeing XMS. One owner means no shared
write concurrency or cross-owner handle authority.

## Inspection and validation

The existing K32RUN01 record layout/version is preserved. Additional u32 offsets:
768 ABI, 772 features, 776 argc, 780/784 heap bounds, 788 current pages,
792 peak pages, 796 allocations, 800 frees, 804 input polls, 808 waits,
812 wait polls, 820 open files, 824 peak files, 828 read bytes, 832 opens,
836 closes, 840 token errors, 844 heap errors, 848 PTE updates, 852 last key,
856 wake flags, 860 elapsed wait ticks, 864 latest token, 872 queued keys,
876 dropped keys. State 7 means waiting; previous terminal states remain intact.
After cleanup current pages and file count must be zero.

`make -j4 -C tools/k386 all test` exercises the shared parser and policies,
including randomized fragmented heaps and user-page permissions. WebNP2's
`verify-k38632-services.mjs` must additionally validate actual CPU contexts,
live page tables, stable wait dispatch counts, real DOS file data/odd copies,
Escape/fault resource cleanup, text scrolling and whole-disk invariance. The
legacy loader, PM32, V86 and GUI jobs remain regression gates.
