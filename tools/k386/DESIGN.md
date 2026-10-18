# KDESK/386 Enhanced architecture (experimental)

## Ownership and boundaries

The existing PC-98 MS-DOS kernel and ROMs remain unchanged. KDESK remains the
real-mode GUI. Internal applications use bounded per-task FIFO queues and a
round-robin cooperative dispatcher. Window focus is resolved between input
events, so switching windows within a keyboard batch preserves the recipient.

K386 is a separately loaded, original TSR (`K386.COM`), not Windows code and not
a browser window manager. An INT 2Fh D386h query returns the versioned, bounded
`K386ABI1` mailbox. Commands create/end, run a quantum, enqueue keys and stop or
resume a session. KDESK polls and presents guest-owned text buffers.

The current execution boundary is **experimental V86 COM sessions**, not a
complete virtualized DOS machine. Each has its own 64 KiB COM address space,
private zero page, 16 KiB PC-98 TVRAM, CPU registers/stack and page directory.
The same 8000h:0100h address maps to different DOS-allocated physical pages.
The monitor's GDT/IDT/TSS/code/data and the host DOS kernel are supervisor-only.
A missing/protected page or unauthorized port access ends the offending VM.

## CPU entry and preemption

A standalone K386CPU first verifies PM entry, #UD, paging/#PF, two V86
executions, #GP INT/CLI interception and real-mode return on the shipped wasm.
The resident monitor uses software context switching and a ring-0 TSS stack.
It resets NT/IOPL/IF on PM entry; real-mode BIOS flags can have NT set and must
not cause IRETD to take a nested task return. All original host flags and
GDTR/IDTR/CR0/CR3 are restored before returning to KDESK.

The monitor requires real mode and an unowned (masked) PC-98 IRQ0. It uses PIT
ports 71h/77h for a bounded one-shot quantum, traps the V86 timer at IDT 08h,
sends a specific EOI, and restores the PIC mask. PIC vectors are not remapped,
so pending keyboard/slave interrupts are retained. It refuses an existing
protected-mode/EMM386 owner or unmasked timer rather than taking its resources.
This timer ownership restriction is a compatibility limit, not full PIC/PIT
virtualization. K386 does not allocate or advertise EMS/XMS memory.

GCC's string operations assume ES=DS. Both stay based on the resident module
in the 32-bit C engine; only explicit FS overrides access guest linear memory.
The assembly boundary saves the real caller's registers, segments and stack.
It clears DF before entering C, retaining the guest DF only in its saved frame.
The ring-0 and real-mode stacks are distinct, with linker-enforced space.
The saved context covers general/segment registers, flags and stack. x87/SIMD
state virtualization is unfinished; CR0 EM/TS/MP prevents those instructions
from sharing hardware state between sessions and #NM ends the offending VM.

## DOS and file access

DOS services run after returning to real mode, with an InDOS guard, never by
calling the original DOS kernel recursively from ring 0 or another V86 task.
The broker temporarily selects the resident PSP so file handles and memory
blocks survive a GUI client exit. Host calls are serialized. Sessions support
a defined subset of INT 21h console, version, time and read/seek/close services.
File creation, writes, deletion and rename are denied. Paths are limited to
A: ASCII disk paths; console/serial/printer device names are rejected so host
DOS reads cannot block the GUI on a real device. Port I/O is trapped.
Zero-length buffered input returns without consuming a key or writing beyond
the two-byte header, including at the end of private RAM.
This prevents concurrent FAT12 writes; it does not establish general DOS/BIOS
compatibility. MZ EXE, COMMAND.COM compatibility, arbitrary resident drivers,
full BIOS/interrupt virtualization and graphics hardware virtualization are not
implemented and must not be described as complete DOS sessions.

## GUI

Ctrl+T/Tasks opens the task manager. Ctrl+N and Ctrl+O open DOS 1/DOS 2. Ctrl+L
or Run edits the COM path; Enter starts it. Ctrl+P/Pause, Ctrl+R/Resume and
Ctrl+E/End control the selected VM. Input goes to the active guest window;
execution quanta alternate independently of focus. Scrollbars/page keys and
horizontal buttons expose the saved 80x25 text screen. Existing external DOS
EXEC tools remain foreground-only; their execution pauses GUI-driven quanta.

K386 absence preserves the ordinary desktop. A separate, explicitly selected
experimental image will be provided, rather than replacing the normal XMS/EMS
configuration. Shared baseline disk images remain immutable build inputs.

## Build and dependencies

`make -C tools/k386 all` builds the probe, TSR, test driver and original COM
fixtures with GNU binutils/GCC; no proprietary assembler is needed. KDESK's
internal C ABI uses register parameters consistently across its objects to fit
the COM segment. Directory data, CG-ROM cache and saved PEGC palette use DOS
memory blocks; the 12 KiB stack assertion remains in force. Object dependencies
must rebuild on C ABI changes. The source pin and copied verifier in webnp2
must match the tested native commit; CPU behavior is tested in the shipped wasm
rather than inferred from the external NP2kai source snapshot alone.

Inspected upstream: `uraraworks/NP2kai-wasm`, wasm branch,
`714f0bef3a24aa419d4cdb0dac9ad02d30855280`. Relevant implementation:
`i386c/ia32/ctrlxfer.c`, `exception.c`, `paging.c`, `task.c`, `cpu_io.c`;
PC-98 PIT/PIC: `io/pit.c`, `io/pic.c`, `bios/bios.c`.
No upstream source changes or Windows resources are included.
