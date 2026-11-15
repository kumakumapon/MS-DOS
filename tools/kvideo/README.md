# Private PC-98 terminal modes

The GUI boot loads `KV98VID.EXE` after K386/K32HOST, before KGUILDR.
Each V86 root gets 128 KiB private planar VRAM and 188 private 16x16 gaiji
(JIS rows 0x76/0x77). A recycled generation starts with zeroed storage. Host CG-RAM,
VRAM and video ports are never written. Without this optional helper the base
K386 profile retains its original backing allocation and port-denial behavior.

Supported: 40/80 columns, 20/25 rows, semigraphic 2x4 cells, digital/analog
16-colour palettes, a bounded standard GDC subset and up to 640x400 graphics.
CGRAM uses A1/A3/A5/A9; text/graphics GDC use 60/62 and A0/A2. Byte IN/OUT
instructions are decoded from the actual private CS:IP, including a single
66 byte prefix. Word I/O, unsupported ports/commands and invalid parameters
fault only that task. Direct stores target private user PTEs; C0000 font shadow
is never mapped into guest code. Protection diagnostics target C0000, since
E0000 is now a legitimate private graphics plane. FIFO-empty and vertical-sync status are bounded
by the monitor ARTIC clock. GDC supports SYNC, START/STOP, PITCH, CURS, MASK,
CCHAR, direction-2 FIGS, word/byte WDAT operations, RDAT/CURD and the first PRAM
partition. Only the first text partition and first graphics partition render.

Run `KVTERM.COM` (or `KVTERM.COM 2` for another private font, or `KVTERM.COM 0` for unregistered gaiji). Keys: g graphics,
a analog palette, c cursor visibility, t text, 4 = 40x20, 8 = 80x25, n normal text attributes,
f deliberately unsupported word I/O, q exit. Both TVRAM planes, VRAM and CGRAM
are changed by actual guest instructions; the browser observer only reads.

The 24 KiB TSR authenticates the real monitor segment and serializes callbacks
on its own guarded stack. Its callback performs XMS transfers only, with far
segment:offset conventional addresses. The core remains below D000, preserving
its 8 KiB protected and 4 KiB real stacks. Internal C uses GCC -Oz/regparm(3);
the assembly kernel_trap boundary explicitly remains regparm(0). Public interrupt,
DOS and snapshot ABIs do not use the internal C calling convention.

Read-only monitor command **16** (15 remains capacity), dedicated GUI syscall
**75**, and fixed 64-byte GuiVideoRead requests expose only typed regions:
0 metadata (128 bytes), 1 font (6016), 2..5 VRAM (32768 each), 6/7 text planes
(8192 each). Chunks are at most 848 bytes inside the existing 1024-byte mailbox.
The native GUI broker checks manager ownership, V86 kind, generation, revision,
reserved fields, region bounds and the complete writable output range. It never
returns native handles or pointers to CPL3. No scheduling occurs during a read.
The desktop commits a complete generation/revision before displaying it, retains
the last accepted display on failed reads, clips to scene damage and window
occlusion, and scales an extended display to fit its viewport. Blank cells are skipped, each graphics byte is decoded once, scaling maps are
precomputed, and cursor-only blinking damages only its cell. Normal 80x25
text still uses the existing 8032-byte v2 snapshot and renderer. All old layouts
remain; the debugger producer only appends fields after its first 212 bytes.

Unit tests cover private fonts, GDC/ROP/bounds, palette ports, semigraphics,
40x20/ring rendering, and rejected read requests. Actual CPU/XMS/PTE, Canvas,
multiple owners, fault/restart and HDD/FD cleanup are verified in webnp2 CI.

Further work: GRCG/EGC/PEGC/SVGA acceleration, multiple display partitions/pages,
additional raster/timing combinations, a complete BIOS mode API and DOS-resident
Japanese FEP. Unsupported CG-ROM port reads are explicit faults; normal Japanese
text continues to use the dedicated read-only GUI font API.

## BIOS, word I/O and two private pages

The additive display extension keeps KvMeta at 128 bytes and KvRead at 1024
bytes. It adds independent A6 CPU access and A4 display selection for two full
128 KiB planar pages. Each V86 backing is 0xe8000 bytes (928 KiB); the low
12 bits of `backing.pages` hold the page count and bit 0x2000 selects the access
page. Page 0 uses A8000/B0000/B8000/E0000 in private backing; page 1 uses the same
four plane addresses plus 0x20000. Only the existing guest VRAM windows are
mapped, to owned XMS, and no writable physical display aperture is exposed.
The desktop's existing terminal cache reads only the displayed page. Creation
zeros both pages; ending or faulting a task releases the full allocation.

KVEXT.COM uses real V86 stores, IN/OUT AX and INT 18h. Keys 0/1 select the
visible page, a changes the first word in access page 0, t exercises BIOS text
mode/clear/cursor, g restores graphics, k tests nonblocking keyboard/reset, b checks refused BIOS mode, f
requests a denied dword I/O and q exits. Argument 0 skips page initialization
so a new generation's zeroed pages can be inspected.

Supported word accesses are the PC-98 low-byte-terminated GDC/page ports 60,
62, A0, A2, A4 and A6: OUT AX uses AL; IN AX returns zero in AH and preserves
EAX's upper half. A 66-prefixed dword instruction is rejected before effects.
KVTERM's f diagnostic now uses that unsupported dword operation. Arbitrary
ports, GRCG/EGC acceleration, PEGC packed modes and SVGA are still refused.

The limited INT 18h display API covers text mode/sense (0A/0B, AL 0..7),
nondestructive display start/stop (0C/0D), text start (0E), bounded cursor
visibility/position (11..13), private TVRAM fill (16, DL character and DH
attribute), graphics start/stop (40/41) and 640x400/selected 640x200 modes (42).
Mode 42 accepts CH C0/D0, 80/90 and 40/50; bit 4 selects display page. These are
bounded modes, not complete BIOS/device emulation. Keyboard AH 01 senses, AH 05
consumes without blocking, AH 03 clears only the private queue and AH 02 reports
no held modifiers; no keyboard matrix or DOS FEP is claimed. The shared metadata
uses flag 64 for access page 1 and 128 for display page 1.

Unsupported BIOS display modes return CF set and an error in AL without changing the mode; unsupported I/O faults only its V86 task.

Page 1 uses C8000h/D0000h/D8000h/C0000h for its four planes. Private glyphs move to A5000h-A677Fh, outside mapped guest RAM and the A4000h-A4FFFh nested-DOS snapshot area. Both full graphics pages and 188 glyphs fit the original 928 KiB backing. CPU PTEs and broker plane addresses share the layout, preserving the 16 MiB mixed V86/PM32 allocation behavior without extra XMS fragmentation.
