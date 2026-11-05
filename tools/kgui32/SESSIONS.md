# KGUI32 text sessions

The CPL3 desktop uses the existing mixed K32HOST/K386 two-slot manager. It
has a separate 4 MiB GUI allocation and never takes an application slot.
PM32 roots retain their existing 4 MiB allocation; configure 16 MiB XMS.
CPU mode switching, paging, DOS/BIOS services and read-only FAT12 policy
remain in the existing backends. There is no new CPU core or DOS kernel.

`scene.c` owns four windows, stacking order, focus, viewport and hit testing;
`desktop.c` owns the ASCII compositor, command editor and snapshot cache.
`sessions.c` is a trusted real-mode broker. Calls alternate with the existing
scheduler only at real-mode boundaries. The hidden/non-focused task gets
normal scheduler slices; STOP explicitly suspends it. Each changed screen
row contributes damage, and the compositor restores overlapping windows.
Resizing/scrolling change only the viewport, never a task's canonical grid.

## ABI 1

Register convention is the existing `k32_call` (EAX service, EBX user pointer,
ECX exact byte count, EAX signed result). Layouts are fixed 32-bit fields in
`gui.h`, version 1 and exact size. Ranges use the loader's page validation;
no monitor pointers, I/O, raw RAM or VRAM are passed to user mode.

69 SESSION accepts `GuiSessionCommand` (152 bytes): version, size, slot (0/1),
generation, operation, key, NUL-terminated printable ASCII path[128].
Operations: CREATE=1, END=2, PAUSE=3, RESUME=4, KEY=5, RESTART=6, FOCUS=8.
CREATE requires generation 0, a non-live slot and a valid path. A foreign
fault is also protected. Only a successful creation grants ownership and
replaces the remembered command. Root generations remain stable across V86
child EXEC/return. Every other mutating operation requires the exact current
generation and ownership. Recycling a slot invalidates the old claim.

RESTART is allowed after end/fault and reuses the last successful root path
and arguments; it creates a new generation. END kills the whole root;
Ctrl+C maps to INTERRUPT instead. ASCII keys pass through; PM32 navigation
keys 256..263 map to PC-98 scan codes. FOCUS key=1 claims input routing for
an owned window; key=0 clears it without changing any task. Key=2 reserves
Escape for the GUI command editor even if the slot is free or foreign;
it grants no task controls. Native F10
always works. Escape is forwarded while a session owns focus and can stop
an uncooperative GUI from a built-in window (the existing spin test).

Results are 0 or negative errors: 1 permission, 2 DOS file not found,
5 monitor I/O, 14 invalid user range, 16 busy, 22 malformed/unsupported,
38 unavailable manager, 116 stale generation. Other backend DOS errors are
negated. Rejected commands have no monitor side effects. Failed CREATE
preserves the old task/view, including a faulted or ended generation.

70 SNAPSHOT accepts `GuiSessionView` (2080 bytes). Input header is version,
size, slot, reserved. Output includes generation, state, backend kind,
exit/fault, counter, cursor, queued-key count, CPU ticks/runs, ownership,
program[16], screen[2000]. It copies data into user memory; screen is
canonical row-major 80x25 ASCII without attributes, not an HTML terminal.
The snapshot can inspect a foreign task but grants no control. It is taken
at a scheduler boundary; GUI and debugger samples across calls need not
represent the same instant.

Exit/fault/forced desktop termination ends only still-owned matching
generations. Other owners survive. Backends reclaim session XMS, mappings,
file handles and heaps; the GUI reclaims its separate graphics/DOS/XMS
allocations. Historical postmortem debugger RAM does not imply live ownership.

## Verification and remaining work

`make test` covers four-window stacking, focus, final release, clipping,
resize/minimize/restore, key routing and a monitor-stub broker policy test
(stale/recycled tokens, foreign owner, failed creation, exact restart).
`KGPROOF.COM` creates two pre-existing owners, EXECs four GUI modes including
`sessionapi`, and checks their generation/CR3/backing/heap and continuing
progress plus conventional/XMS free space. `sessionapi` executes invalid
pointer/header/size/slot/operation, read-only and stale-generation calls in
actual CPL3 and attempts all foreign-owner controls.

WebNP2's `verify-kgui32-sessions.mjs` exercises KSH+KSH, KSH/COUNTER+COUNT32
and VIEW32+COUNT32 with actual CPU frames, separate CR3/backing, private RAM
samples, per-session screen/cursor/keys, file reads, background execution,
stop/resume/minimize, failed load, #PF, restart, resize/drag and full disk
hashes on PEGC/Cirrus. The foundation suite retains low-memory, GUI #PF/spin,
foreign-owner reclamation and original KDESK calculator regression checks.

This is the first text-client milestone. Generic graphical GUI clients,
widget SDK, Japanese/attribute rendering, clipboard, direct-GDC application
virtualization, shared disk writes and expansion beyond two slots remain
later work. Run uses A: root files only under existing backend restrictions.
