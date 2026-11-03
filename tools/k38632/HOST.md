# PM32 resident tasks and KDESK windows

`K32HOST.EXE` is a separate real-mode resident module. INT 2Fh AX=D390h,
BX=0 returns AX=390Dh and ES:DI pointing to K32HOST1 version 1. BX=1 executes
one mailbox request. The mailbox checks a real-mode caller, masked/unowned
IRQ0, InDOS, a busy guard and a slot index before DOS operations. The service
switches to its owning PSP for DOS handles and restores the caller PSP.

There are two private task slots. Each has a trusted conventional-memory
snapshot of `.task`, a distinct aligned three-page directory/table allocation,
and an owned XMS block (4 MiB plus alignment allowance). The shared module
contains code, private entry/exception stacks and temporary transfer buffers;
those stacks are never snapshotted. A20 is acquired/released for each PM entry,
rather than for the entire lifetime of a task. Cleanup releases only the selected
owner; global XMS free/largest values may legitimately change while peers live.
The original standalone K32RUN and its ABI 0100h/0101h remain available.

Requests: create, step, key, stop, resume, end, poll. Step performs at most one
Ring-3 syscall/IRQ quantum and returns to real mode. Poll checks a pending wait
without entering user code. Key input comes from the caller's selected task
queue, never from direct BIOS consumption. Stops retain pages/handles; timeout
remaining time is frozen while stopped. End/exit/fault revoke PTEs, close owned
files and reclaim XMS and conventional snapshots. A failed close/free retains
ownership and refuses reuse until cleanup succeeds.

A private 80x25 console consumes stdout, CR/LF/BS/TAB and bounded CSI H/f/J
sequences. Host view records publish actual CPU-produced frames, PTE samples,
data samples and ownership counters after the API call. K32HOST's MZ tag marks
it as a native real-mode service so V86 loaders reject it before replacement.

Initial bring-up: install K386, then K32HOST, leave KDESK with F10 and run
K32MIX.COM. The diagnostic first alternates a real PM32 SPIN32 with a V86
COUNTER, then alternates two PM32 SPIN32 owners. It checks actual user frames,
progress, different page directories/backing and host restoration before cleanup.
The version-2 K32MIX01 record also compares the largest free conventional DOS
block before/after the proof. SPIN32 makes no voluntary syscall: IRQ0 must
preempt it. Browser verification repeats the proof after all GUI task lifecycles.

## Shared real-mode manager

AX=D391h / BX=0 discovers K32MUX01 version 1 (200 bytes); BX=1 executes the
legacy command/key/path offsets through the new manager. It owns the two-slot
resource policy, budgets and journal, and exposes pointers to the actual V86 or
PM32 producer for each view. AX=D393h returns AX=393Dh / BX=occupied PM32 mask;
the small legacy CREATE guard refuses a slot already owned by PM32. Raw PM32
CREATE similarly refuses a live V86 slot. A manager transaction may prepare a
candidate before ending a V86 owner; failure preserves the previous owner.

The original V86 monitor stays within its 64 KiB image/stack bounds. Its only
extensions are the ownership guard, a no-user-dispatch clock poll, and diagnostic
pointer discovery (AX=D388h). The two execution engines keep their mode-specific
frames and exception handlers. Every slice restores real mode before another
engine or DOS broker runs. No Windows API or hardware I/O emulation is added.

The common manager reuses the existing weighted scheduling/accounting policy.
Only runnable tasks accrue credits. Waiting PM32 tasks receive a bounded poll,
not a BIOS polling loop; GUI input is routed by the active slot. File operations
remain serialized in real mode and all PM32 handles remain read-only. V86
backend events, including child EXEC exit/fault restoration, are transferred to
the common journal with the common task generation.


## KDESK usage

The experimental image installs K386, K32HOST, then KDESK. Without K32HOST the
GUI falls back to the legacy V86 API; without K386 it remains the ordinary GUI.
Open Find apps (Ctrl+K): **PM32 viewer** starts `VIEW32.EXE VIEWDATA.TXT`,
**PM32 ticker** starts `COUNT32.EXE 1`, and **PM32 memory test** starts MEM32.
A PM32 launch uses a free shared slot. End a live owner before reusing that slot.
Ctrl+N/Ctrl+O focus slot 1/2; Ctrl+L accepts e.g. `COUNT32.EXE 2`,
`VIEW32.EXE VIEWDATA.TXT`, or `KSH.EXE` after End. PM32 windows display their
program name and coexist with Calculator, Clock and Memo. Move, resize, tile,
maximize, minimize, restore and taskbar switching use existing native controls.
Resizing changes the viewport over a fixed 80x25 console, not program geometry.

Ctrl+P pauses, Ctrl+R resumes, and Ctrl+E ends the selected owner. Tasks (Ctrl+T)
shows generation, program, priority/budget, CPU usage, XMS, heap and handles.
Its R/Start resumes a stopped owner or restarts the retained PM32 command after
exit/fault/End. A restart gets a new generation. Focused ASCII/packed PC-98
navigation keys enter only that task's queue. VIEW32 uses n/p, j/k, PageDown/
PageUp, Home/End and q. Escape cancels the selected PM32 task. Ctrl+C/Break ends
a PM32 root; V86 child interruption still returns to its shell.

Normal exit and user faults immediately close owned files, revoke page mappings
and release that PM32 task's XMS/tables/snapshot; peers retain theirs. The last
screen, command, CPU frame and diagnostics remain available until reuse. Pause
retains all allocations. A PM32 candidate that fails before publication preserves
a V86 owner and its generation, context and backing. Simultaneous FAT12 writes
are disallowed: only serialized read-only broker operations are provided.

## Interface and bounds

K32MUX01 has the original legacy command/key/path offsets through byte 151.
Following fields are two u16 kinds at 152 (0=V86, 1=PM32), two u16 ABIs at 156,
two u32 generations at 160, view linear pointers at 168, retained PM32 record
pointers at 176, resource pointer at 184, legacy mailbox at 188, actual V86
diagnostics at 192, and u16 owner PSP/reserved at 196/198. Commands retain legacy
1..11; POLL=12 services one wait without running user code; RESTART=13 uses the
stored PM32 command or KSH. Invalid slot/command returns 87. A live PM32 slot
refuses replacement with error 5; clients should End first. Error 8 reports
insufficient DOS/XMS allocation. The version/size and busy flag are mandatory
for readers. D393h is an ownership query, not a task execution API.

The K32HOST1 raw mailbox version 1 is 7584 bytes. It has two 3200-byte views at
160 (2044-byte K386View, generation, command[128], record[1024]); a complete
failed-create record begins at 6560. PM32 record additions are sixteen CPU-read
RW dwords at 880, sample count at 944 and page-table reclamation flag at 948.
K32MUX's K32SYS01 v1 resource structure reuses the legacy 1084-byte schema but
has its own authoritative generations and journal; legacy K386SYS1 is unchanged.
PM32 xms reservation is 4100 KiB per live owner, including alignment. Code,
startup, stack and heap share the 4 MiB user budget. Heap/file limits and ABI
0100h/0101h remain in SERVICES.md and SDK.md. Native runtime metadata consumes
additional conventional memory; GUI image buffers may have less space available.

The GUI classifier reads only a bounded tagged header; the PM loader validates
the complete container/ELF before execution. Ordinary DOS and native graphical
EXEs still use their existing execution path. Arbitrary DOS/BIOS compatibility,
Windows APIs, DPMI, PM32 graphical/widget APIs, x87/SIMD, shared writable files,
more than two owners, dynamically sized consoles and direct GDC/PEGC app
virtualization are outside this milestone. Foreground real-DOS EXEC suspends
GUI-driven scheduling until it returns. A synchronous DOS read can exceed the
10 ms scheduling target; the manager starts no next slice after that call.

## Build and verification

```
make -C tools/k386 all test
make -C tools/window all test
```

WebNP2 pins the native SHA in prepare-msdos4-cpanel-ci.sh and builds both image
profiles from it. Its new verify-k38632-windows.mjs runs in PEGC and Cirrus modes,
checking actual CPU contexts/CR3/PTEs/private data, background progress, focused
input, blocked dispatch/accounting, window operations/GUI apps, stop/resume and
restart generations, #PF/#GP containment, PM32+V86 and two-V86 operation, sixteen
real read-only handles and per-owner recovery. It compares DOS/XMS availability
after teardown and every byte of the exported FAT12 image, and checks a real
4 MiB failure preserves the previous V86 owner. Existing standalone PM32 and
V86/GUI suites remain separate regression gates. Exact execution/CI results
belong to PR #36 in both repositories, not to documentation claims.
