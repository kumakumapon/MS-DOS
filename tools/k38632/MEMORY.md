# K38632: XMS-backed memory and resumable PM32 execution

`K32MEM.EXE` is a standalone DOS MZ program with an original protected Ring-3
execution layer and a built-in 32-bit payload. It implements the memory and
resume milestone of Issue #31. It is separate from the resident K386/V86
monitor and leaves the original DOS kernel and emulator core intact.

## Build and use

```
make -j4 -C tools/k386 all test
```

Use the experimental WebNP2 image with 16 MiB, FDXMS286 and no EMM386. Save
documents and exit KDESK with F10. At real DOS run `K32MEM`; successful execution
prints `K32MEM DONE: resources reclaimed` and returns to the prompt. `KDESK`
restarts the normal desktop. Existing PE/V86 ownership or unmasked IRQ0 is
refused before XMS allocation; launching from a live KSH window returns a refusal.

| Argument | Built-in payload | Expected outcome |
| --- | --- | --- |
| none | Verify zeroes, write/verify all 2 MiB and checksum | Normal exit after many IRQ0 resumptions |
| F | Read unmapped data guard | #PF, error 4 |
| C | Write user code | #PF, error 7 |
| K | Read monitor memory | #PF, error 5 |
| S | Push into stack guard | #PF, error 6 |
| P | Execute CLI at CPL3 | #GP, error 0 |
| T | Infinite loop, 64-quantum budget | Scheduler terminates at the budget |
| B | Infinite loop | Escape terminates from the real-mode scheduler |

Fault/cancel cases are intentional tests and still print DONE when the expected
event and reclamation were verified. Unexpected outcomes print FAIL. The default
maximum is 65,536 quanta; it also prevents an unintended endless diagnostic.
On a 4 MiB WebNP2 machine, allocation is refused because the 4 MiB budget cannot
fit in free XMS; no protected execution occurs and free memory is retained.

## Ownership and address space

The runner confirms its 64 KiB DOS module allocation before using private stacks
or clearing BSS. Page tables, TSS, context and records are inside that allocation.
The real stack is at FFF0h, the protected stack at EFF0h, and BSS ends below D000h.

Real-mode XMS calls allocate **4 MiB plus a 4 KiB alignment allowance**, lock the
handle and validate its physical range below 16 MiB. The aligned 4 MiB budget is
zeroed before the payload is published. Local A20 enable/disable is balanced;
preexisting A20 state is checked after cleanup. The DOS PSP, XMS handle, lock,
physical base and allocated bytes are recorded. The diagnostic has one owner.

| User linear address | Mapping |
| --- | --- |
| 800000h | 4 KiB read-only code |
| 801000h–A00FFFh | 2 MiB read/write array |
| A01000h | Unmapped data guard |
| A02000h | 4 KiB payload control/results |
| A03000h | Unmapped stack guard |
| A04000h | 4 KiB read/write stack, initial ESP=A05000h |

The low identity map is supervisor-only. This milestone reserves a 4 MiB
physical budget but exposes only the mappings above; it has no general heap API.
Paging metadata and alignment are additional overhead, not user payload capacity.
The 386 has no NX; data execution prevention is not claimed. x87/SIMD contexts
are unsupported; CR0.EM/TS prevent unvirtualized x87 execution.

On completion, fault, Escape or budget termination, the caller CR3 and real mode
are restored, user PDE/PTEs are revoked, A20 ownership is released, the XMS handle
is unlocked and freed, and the driver's free/largest block values are checked.
Cleanup errors retain their handle/lock evidence and print FAIL rather than
reporting successful reclamation. A failed allocation never enters PM.

## Execution and interfaces

`runtime.S` owns XMS lifetime, paging, traps, scheduling and DOS return.
`memory-app.S` is the built-in Ring-3 payload. `runtime.ld` and the existing
`tools/mz/build.py` provide the DOS loader/allocation contract.

IRQ0 saves all eight GPRs, DS/ES/FS/GS and EIP/CS/EFLAGS/ESP/SS to a private
76-byte context. Execution returns to real mode between slices; the next entry
restores that actual frame and resumes with IRETD. The payload uses ordinary
32-bit pointers across the complete 2 MiB array, retaining register sentinels.
The kernel samples real data around 64 KiB, 1 MiB and 2 MiB boundaries.

The DPL3 INT30h gate ends the payload with EAX=0. It is an internal execution
contract, not a published application SDK. DOS keyboard checks and every XMS
call occur only in real mode. IRQ0/#DF use the private GDT/IF contract documented
in README.md; the PIC vectors are preserved. Every return checks CR0/CR3,
full-width GDTR/IDTR, flags and PIC mask before using DOS.

## CPU record ABI

Signature `K32MEM01`, version 0100h, size 1024 bytes. A live address must equal
the recorded program base plus linked offset at 504. A wrapping ARTIC-clock tag
distinguishes repeated runs. Records are nonresident; inspect before another
DOS program reuses their memory. Unless noted, fields are little-endian u32.

| Offset | Meaning |
| --- | --- |
| 12/16 | State / unexpected error |
| 20/24/28/32/36 | Program base / PD / aligned physical base / raw base / allocation bytes |
| 40/42/44/46 | Handle / locked / owner PSP / argument (u16 each) |
| 48/50/52/54 | Free before/after, largest before/after (u16 KiB) |
| 56/60/64/68 | Clock tag / IRQ count / resume count / terminal reason |
| 72/76/80/84/88 | CR2 / fault error / PIC ISR / protected CR0 / protected CR3 |
| 92/96/100/104 | Returned CR0 / saved CR0 / saved CR3 / returned CR3 |
| 108/112 | Saved/returned flags |
| 116/122/128/134 | Saved/returned GDTR and IDTR, 6 bytes each |
| 140/141/142 | Saved/returned PIC mask (u8), owned A20 enable (u16) |
| 144/148/152/156/160 | Payload stage / checksum / sample count / freed handles / attempts |
| 168/244/320 | First IRQ, last IRQ, final 76-byte contexts |
| 396 | 16 actual dword data samples |
| 464/468/492 | User mappings revoked / payload loop counter / quantum budget |
| 500/502/504 | A20 before/after (u16), linked record offset (u32) |
| 512 | 13 actual PDE/PTE samples captured before revocation |

States: 0=new, 1=running, 2=suspended, 3=exited, 4=faulted, 5=terminated,
6=allocation refused. Reasons: 48=exit gate, 32=normalized hardware IRQ0,
13/14=CPU exceptions, 65=Escape, 66=budget. Hardware IRQ0 still uses vector 8.
Context dwords: GS, FS, ES, DS, EDI, ESI, EBP, ignored PUSHAD ESP, EBX, EDX,
ECX, EAX, normalized vector, error, EIP, CS, EFLAGS, user ESP, SS.

## Validation and remaining work

Binary tests check MZ allocation, privilege gates, descriptor-register widths,
payload size and record/page/stack bounds. WebNP2 `verify-k38632-memory.mjs`
executes all modes, validates real CPU frames, direct data samples and checksum,
actual page tables and their revocation, restoration, actual XMS scarcity,
repeated reuse, KDESK/Memo/two V86 counters after recovery, live V86 refusal,
and whole-disk SHA-256 invariance. CI stores JSON and screenshots.

This is **one standalone PM32 owner with an embedded diagnostic**, not an
external ELF32 loader, SDK, simultaneous PM32/V86 scheduler, KDESK PM32 window,
or multi-task allocator. Those remain later milestones of Issue #31.
