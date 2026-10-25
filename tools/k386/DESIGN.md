# KDESK/386 Enhanced architecture (experimental)

## Ownership and scope

KDESK is the real-mode PC-98 GUI. Its internal applications retain their
cooperative event queues, task IDs, focus, task manager, graphics and mouse.
K386 is an independent original TSR, not Windows code or a browser window
manager. NP2kai supplies the CPU, memory and PC-98 hardware; the guest supplies
all scheduling, DOS services, processes, input routing and window contents.
Neither the historical DOS kernel/COMMAND source nor the emulator is modified.

Two V86 sessions execute background CPU quanta regardless of GUI focus. They
have independent conventional RAM, CPU/stack, IVT, text VRAM, directories and
search state. The supported environment is still a **read-only DOS subset**,
not arbitrary DOS/BIOS/TSR or external graphics compatibility. KSH remains the
default shell. The bundled original COMMAND.COM is also tested explicitly;
success of a command does not establish general COMMAND compatibility.

## Modules and interfaces

- `monitor.S`: caller preservation, GDT/IDT/TSS, protected/V86 entry, traps and
  real-mode return. Real and protected stacks are distinct and linker-checked.
- `engine.c` (32-bit): guest instruction traps, private console, DOS/BIOS
  dispatch, virtual IF, interrupt delivery, text/context/RAM snapshots.
- `monitor.c` (16-bit): INT 2Fh D386h mailbox, InDOS/ownership guards, XMS A20
  ownership, CPU quantum and serialized real-mode broker dispatch.
- `backing.c`: allocate/lock/free XMS and bounded moves, with alignment and
  physical-address checks. A20 is locally enabled only around each quantum.
- `arena.c`: trusted first-fit paragraph allocator, owners, resize and release.
  Guest-writable DOS MCBs are a mirror, never authority for host operations.
- `loader.c`: bounded COM/MZ header, entry/stack/allocation and relocation checks.
- `runtime.c`: private RAM, PSP/environment, EXEC process stack, inherited file
  handles, private search state and serialized read-only host DOS services.
- `resources.c` / `report.c`: guest weighted scheduler, execution budgets,
  typed resource records, bounded lifecycle journal and compact diagnostic text.
- `tools/window/k386_client.c`: mailbox client and monitor-scheduled GUI polling;
  the GUI never becomes a host-side process manager.

See [RESOURCES.md](RESOURCES.md) for resource-control usage, API, timing units,
interruption semantics and limits. Older monitors retain alternating polling.

`K386ABI1` version 1 remains 4240 bytes. Commands create, step, input, stop,
resume and end either session. Each 2044-byte view contains state, last CPU
status, CR3, physical active-PSP address, screen and active PSP (offset 42,
formerly reserved). Mailbox word 22 is nonzero while an API mutates views;
debuggers wait for zero before taking a coherent snapshot. Only the foreground
window receives keyboard input.

`K386DOS1` diagnostics version 2 is 2348 bytes: last 16 DOS/BIOS calls per VM,
actual 76-byte saved CPU frames and 512 bytes read through each active CPU
mapping beginning at its PSP. These bounded snapshots are debugger evidence,
not an execution interface. NP2's existing 2MiB main-RAM debug buffer contains
font/graphics storage above 1MiB, not the separate extended RAM allocation;
browser tests must not mistake that storage for XMS. Page tables in conventional
RAM are inspected directly; PSP/module snapshots come from the active mapping.

## CPU, paging and isolation

Each session owns a locked, aligned XMS backing for 640KiB conventional RAM,
16KiB TVRAM and private EXEC metadata. Mapped guest conventional addresses are
00000h..9FFFFh; A0000h..A3FFFh is private PC-98 text storage. Free paragraphs
remain mapped, as DOS programs such as COMMAND retain a transient area after
shrinking their allocation. Different sessions never share these physical pages.

The low page table starts as supervisor identity mappings. A second supervisor
identity table provides linear aliases at 00400000h..007FFFFFh. The monitor's
32-bit CS/DS/SS, GDT/IDT/TSS and stack use these aliases while all low conventional
pages are replaced by user mappings to the selected XMS backing. PD/PT storage,
the real DOS kernel and the other VM remain inaccessible to V86 code.

Actual PC-98 firmware F0000h..FFFFFh is mapped read-only. This allows the
original COMMAND's model-byte probe without exposing host DOS RAM. Other
hardware apertures stay supervisor-only. Port I/O is trapped; outside accesses
or ROM writes fault the offending VM. No IBM hardware emulation is introduced.

Exit restores low monitor mappings, jumps to a low CS, reloads GDTR/IDTR with
low linear addresses, then disables paging. Forgetting the descriptor-table
addresses here caused a real triple-fault/reboot during development; the
corrected sequence is exercised by every tested quantum and fault return.
Original CR0/CR3, GDTR/IDTR and host flags are restored. NT/IOPL/IF are sanitized
on entry so IRETD cannot accidentally perform a nested task return.

The monitor requires real mode and an unowned, masked PC-98 IRQ0. It owns a
bounded PIT one-shot (71h/77h), handles IDT 08h as IRQ0 or #DF, acknowledges it
and restores the PIC mask without remapping vectors. The CPU can therefore
preempt even an infinite guest CLI loop. Existing EMM386/protected-mode or
unmasked-timer ownership is rejected. The real-mode FDXMS286 driver coexists;
K386 consumes XMS but does not offer an XMS/EMS API inside its VMs.

GCC's engine uses ES=DS at the high monitor base. Only explicit FS accesses
read/write guest linear RAM. DF is cleared before C/string code while the saved
guest flags retain it. x87/SIMD are not virtualized: CR0 EM/TS/MP causes an
isolated #NM rather than sharing that hardware state.

## DOS memory and processes

The first guest arena is data segment 1000h through the end of conventional RAM
A000h (initial MCB at 0FFFh). Up to 32 trusted MCB records support AH=48h/49h/4Ah,
first-fit allocation, ownership checks, contiguous resize and free coalescing.
A forged guest MCB does not change allocation authority. MCB exhaustion or lack
of space returns DOS error 8, with the largest available block where applicable.

Root COM normally starts at PSP 8000h with 64KiB; the allocator chooses another
address for large MZ or child programs. COM is at most FF00h bytes. MZ module,
minimum allocation and stack must fit the private arena (at most 90000h bytes),
with valid relative entry and relocations and no overlay. MZ is loaded at
PSP+10h with DS/ES=PSP; the initial block covers its required minimum/stack.
The header's larger maximum allocation is validated, not eagerly allocated.

Each PSP has parent, end segment, environment, JFT, INT 21h/RETF stub, blank FCBs
and standard command tail. Environments are copied and owned by the child,
bounded to 2KiB input plus the DOS program-name trailer. Default variables
include COMSPEC=A:\COMMAND.COM, PATH and PROMPT. AH=29h FCB parsing uses the
original real-mode DOS parser with copied buffers, never guest/host pointers.
The initial FCB contents are not a full EXEC parameter-block implementation.

Up to four simultaneous process levels (root plus three child EXECs) live in
one VM's DOS arena. The parent retains its allocated memory. CPU/stack, PSP,
DTA/search continuation and virtual IF are saved; the IVT is copied to hidden
XMS metadata. A child inherits a copy of its parent's read-only handles via
real DOS duplicates, sharing the file cursor as DOS does. On normal exit or a
contained fault, child-owned blocks/handles are released and parent metadata,
IVT and registers are restored. AH=4Dh returns the child result. An over-depth
EXEC returns error 8 without destroying the parent. GUI Run replaces the root.

There are 16 virtual handles, including private standard CON handles and NUL.
AH=45h/46h duplicate handles; all disk handles are read-only. CON/NUL open/read/
write/IOCTL are emulated without passing real console devices to host DOS.
Disk reads honor the entire requested 16-bit length in bounded broker chunks;
the former 4096-byte truncation prevented COMMAND loading its transient area.

## DOS, interrupts and files

Real DOS is entered only after leaving protected mode, with an InDOS guard and
the resident PSP selected. Host calls are serialized. Read/seek/close, private
cwd, attributes, date/time, FCB parsing and directory search use copied data.
Host paths are canonical ASCII A: disk paths. Other drives, host devices,
file creation/write/delete/rename and attribute changes remain denied. This
explicit restriction prevents concurrent FAT12 writes, not general DOS/BIOS
compatibility. DTA reserved continuation bytes must match the broker-issued
state before a host FindNext; each process saves its own trusted continuation.

PUSHF/POPF/IRET (16/32-bit), CLI/STI and private software vectors virtualize the
CPU's logical IF. Real IF remains enabled in V86 for monitor preemption.
INT 18h/1Ch/20h/21h/29h/2Fh are reserved services. Partial NLS/country and DOS
message-pointer services return private guest tables/pointers, never host ones.
They do not establish full Japanese DOS/BIOS compatibility.

PC-98 INT 1Ch AH=02h/03h provides a private one-shot 100Hz interval timer using
private vector 07h and BDA 058Ah. Clock samples use the actual PC-98 ARTIC
counter. Expiry waits while virtual IF is clear, wakes an input-waiting VM when
IF permits, and pushes an IRET-compatible frame on its private stack. A callback
can rearm itself. Invalid callbacks are contained faults. Pause freezes virtual
timer progress; killing a process clears its owned timer. AH=00h calendar reads
use copied real BIOS data. Setting the host calendar is denied. This is not full
PIC/PIT/IRQ/BIOS virtualization, and callbacks must respect DOS non-reentrancy.

## GUI and boot

Ctrl+N / Ctrl+O open DOS 1 / DOS 2 and start KSH. Ctrl+L/Run replaces the root
program. Ctrl+P/Pause, Ctrl+R/Resume and Ctrl+E/End control it. Ctrl+T opens the
task manager. Text windows retain move/resize/minimize/restore and scrollbars.
Waiting VMs are polled for timer events without marking unchanged views dirty.
Foreground external tools run through ordinary real DOS EXEC and pause GUI
quanta while they own the desktop. Direct GDC/PEGC applications are not yet
virtualized into concurrent DOS windows.

Normal KDESK works without K386. The separate experimental boot profile loads
FDXMS286 (no EMM386), installs K386, and starts KDESK automatically. WebNP2's
experimental link selects 16MiB extended memory; the ordinary profile is kept.
Existing saved images are not rewritten by the importer or by read-only VMs.

## Build, validation and provenance

`make -C tools/k386 all test` builds the monitor, loader/arena tests and original
fixtures with GNU GCC/binutils. `make -C tools/window all test` builds KDESK and
its existing app tests. KDESK.EXE uses independent code and data/stack segments,
an MZ data-segment relocation and a 12 KiB stack reservation. Both near-address
segments are limited to 64 KiB. The monitor's own stack space is separately asserted.

K386.EXE keeps its real/protected-mode code and data in one 64 KiB module to
preserve the existing descriptor/mapping contract. DOS allocates a separate PSP
before that module; resize and TSR paragraph counts include that PSP. Broker
calls switch DOS ownership to the PSP returned by INT 21h/62h, not to the monitor
segment. The MZ header requests an exact bounded allocation instead of all free
memory. The resident monitor remains loaded after KDESK exits.

`tools/mz/build.py` packages linker-defined load/memory bounds and relocations.
KDESK's child EXEC uses its actual PSP for default FCBs and its data segment for
the command tail and parameter block. COM image tools discover their suspended
parent's clipboard within the parent-owned MCB, including the separate data
segment. The original COM startup/linker remains available to those tools.

webnp2 pins the tested full native SHA in `scripts/prepare-msdos4-cpanel-ci.sh`.
That importer builds the ordinary DOS image, then a separate K386 image from the
same source. Manifest source SHA and image hashes travel with CI artifacts.
`verify-k386.mjs`, `verify-k386-shell.mjs` and `verify-k386-processes.mjs` inspect
actual contexts, page tables, PSP/modules, private RAM fixtures, directory/file
results, background execution and exported disk hashes in Chromium/Playwright.
The ordinary GUI verifier is retained. No synthetic browser DOS processes exist.

Inspected upstream: uraraworks/NP2kai-wasm wasm branch,
714f0bef3a24aa419d4cdb0dac9ad02d30855280; CPU `ctrlxfer.c`, `segments.c`,
`exception.c`, `paging.c`, `task.c`, `cpu_io.c`; PC-98 `bios/bios1c.c`,
`romimage/BIOS/VECT08.X86`, `io/pit.c`, `io/pic.c`. The shipped wasm is tested
rather than assuming every upstream capability works. Original licenses and
source provenance are retained; no Windows code, binaries or resources are used.


## Interactive scheduling and presentation

The scheduler services at most eight slices or 10 ms of PC-98 free-running
clock time per desktop call, whichever limit is reached first. It re-evaluates
runnable sessions, weighted priorities, virtual timers and CPU quotas between
slices. Host DOS calls remain serialized. A slow synchronous host file request
can exceed the time target; no further slice starts after that request returns.
Blocking input and explicit yields still return from each protected-mode slice.

KDESK batches a session screen snapshot and caches rendered viewport cells.
Unobscured DOS windows repaint only changed character spans, state labels and
scrollbar positions. Window overlap, dialogs and dragging use normal scene
composition; geometry and focus operations rebuild the cache. Emulator clock
settings and the isolated, read-only file service policy do not change.
