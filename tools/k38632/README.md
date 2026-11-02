# K38632 first milestone: real Ring-3 CPU bring-up

`K32CPU.COM` is an independent original PC-98 DOS diagnostic for issue #31.
It executes actual 32-bit Ring-3 instructions with PE/PG set and VM clear,
handles a user-callable exit gate, timer preemption and exceptions, and returns
to DOS. It neither installs a PM32 runtime nor changes the resident K386.
The normal V86 runtime, original DOS kernel and NP2kai core are unchanged.

## Build and run

```
make -C tools/k38632 all test
# Also built/tested by make -C tools/k386 all test.
```

Use the experimental WebNP2 disk (16 MiB, FDXMS286, no EMM386). Exit a clean
KDESK with F10; at the real DOS prompt run `K32CPU`. A successful run prints:

```
K32CPU PASS: Ring3 PM32, paging, IRQ0, PF, GP, DF, DOS return
```

Run it again, or run `KDESK` and open the normal DOS sessions. The diagnostic
requires the standard PC-98 IRQ0 vector 08h, real mode and masked/unowned IRQ0.
It refuses an existing PE/V86 owner or unmasked IRQ0 before modifying hardware.
Invoking it from KSH inside a V86 window deliberately refuses, then returns to
its shell; run it from real DOS for the protected-mode experiment.

## What is exercised

| Case | User operation | CPU result |
| --- | --- | --- |
| 0 | `INT 30h` through a DPL3 gate | CPL3 -> CPL0 -> real DOS |
| 1 | Infinite loop incrementing private data | Actual PC-98 PIT IRQ0 preempts CPL3; PIC ISR bit 0 is set |
| 2 | Read unmapped linear C00000h | #PF, user-read/not-present error 4, CR2=C00000h |
| 3 | `CLI` at CPL3 / IOPL0 | #GP, error 0 |
| 4 | Read unmapped memory with the #PF gate not present | #PF delivery raises #NP and the CPU generates #DF, error 0 |

Each case separately enters/leaves PM and paging. Its trap frame and CR0/CR3,
CR2, directly sampled user CS/SS/flags/ESP, user data and host return state are
recorded. The program verifies the records before printing PASS. Entry clears
NT/IOPL; Ring-3 flags have IF set and VM clear. CPL3 cannot call the DPL0 fault
gates to fabricate the exception cases. No DOS/BIOS or disk call runs in PM.

The user uses flat 32-bit selectors but only three physical 4 KiB pages from
this DOS allocation. Linear 800000h is user read-only code, 801000h is user RW
data, 802000h is an unmapped guard, and 803000h is a user RW stack. Low identity
mappings remain supervisor-only. This proves paging/privilege transitions; it
is **not** a multi-megabyte application allocator, loader, SDK or window API.
No x87/SIMD context support, DPMI or Windows executable compatibility is added.

## IRQ0 and double fault on PC-98

The probe preserves the PIC vector configuration. Both IRQ0 and #DF enter IDT
08h; VM=0 is intentional in both cases and cannot distinguish them.

IRQ0 pushes EIP/CS/EFLAGS. #DF first pushes error=0, placing CS where the IRQ
frame contains EFLAGS. In this probe's private GDT, valid code selectors are
08h and 3Bh, both below 0200h. All Ring-0 gates clear IF, so IRQ0 can only arrive
from the Ring-3 context whose saved IF is set. The entry checks saved IF and the
exact user CS before normalizing the IRQ frame. It corroborates IRQ0 with the
actual PIC in-service bit. #DF uses its real error-code frame and is contained
by returning through a known supervisor stack rather than resuming user code.

This classifier depends on this private GDT/IF contract, checked by the binary
layout tests. It is not a generic classifier for arbitrary descriptor tables,
kernel interrupt nesting or arbitrary PIC layouts. A production PM32 runtime
must retain or replace these explicit invariants.

Only an unowned PIT channel is used. The IRQ is acknowledged and the original
PIC mask is restored. PIC initialization/vector registers are not rewritten;
the diagnostic reads ISR and restores IRR read selection. CR0/CR3, GDTR/IDTR,
caller flags and real-mode FS/GS are restored. A20 is not changed: physical pages
are below 1 MiB even though their user linear addresses are above it.

## Record format

`K32CPU01`, version 0100h, total 576 bytes, lives in conventional memory.
Offsets 8/10 are 16-bit version/size. Header words below are 32-bit unless noted:

| Offset | Meaning |
| --- | --- |
| 12/16 | completed bitmap / error (zero is success) |
| 20/24 | physical program base / page directory |
| 28/32, 36/40 | original/restored CR0, original/restored CR3 |
| 44/50, 56/62 | original/restored GDTR and IDTR (6 bytes each) |
| 68/72 | original/restored caller flags |
| 76/77/78 | original/restored PIC masks (bytes), refusal reason (word) |
| 80/84/88 | case count, linked record offset, ARTIC hardware-clock tag |

Five 96-byte records start at offset 96, with 24 32-bit fields in order:
reason, CR0, CR3, CR2, EIP, CS, flags, ESP, SS, CPU error, direct user CS,
user SS, user flags, user SP, cookie, PIC ISR, user DS, case index, restored
CR0, restored CR3, restored flags, restored mask, #PF gate access, saved flags.
Reasons are 48 (INT30), 32 (normalized IRQ0 reason, **hardware vector is 8**),
14 (#PF), 13 (#GP), 8 (#DF). The clock tag distinguishes consecutive browser
invocations; it is a wrapping hardware counter, not a globally unique ID.
The address must equal program base plus the linked record offset, excluding
uninitialized file-buffer copies. The program is not resident: inspect records
at the DOS prompt before another program reuses its memory.

## Verification and remaining scope

`test_probe.py` checks linked GDT privilege/size bits, actual IDT targets/DPLs,
user-page code size, record layout and real/protected stack allocation bounds.
WebNP2 `verify-k38632.mjs` checks real CPU records/page tables, repeated probes,
IRQ0 versus genuine #DF, restoration, a full KDESK restart with two background
V86 counters and native Memo, live V86 refusal, another probe after teardown,
and every byte of the FAT12 image. Normal K386/XMS/XARRAY/GUI suites remain gates.

The first implementation bug wrote through the protected code selector in the
exception entry, causing recursive faults/reboot. The corrected entry saves
state through writable supervisor SS; actual Ring-3/fault/return tests cover it.
The existing resident V86 transition path is not modified.

Issue #31's PM32 loader/SDK, task-specific XMS page allocation, simultaneous
PM32/V86 scheduling and PM32 GUI API are later milestones. Passing this probe
does not mark those features implemented.
