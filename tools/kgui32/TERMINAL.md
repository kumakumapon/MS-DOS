# DOS terminal: PC-98 characters and attributes

The independent GUI renders the private V86 80x25 character and attribute
planes, rather than treating the low byte of a character word as ASCII.
ASCII, JIS X0201 half-width kana and JIS X0208 Shift-JIS output use CG-ROM
glyphs. A PC-98 kanji lead consumes two cells; both halves use their own
attributes. Horizontal clipping may show either half, without decoding a trail
as another character. An orphan at column 80 is shown as `?`.

DOS INT 21h output (02h/06h/09h/40h), INT 29h and console echo share a bounded
per-session SJIS/ANSI parser. Split pairs survive calls and yields. Invalid
pairs show a replacement and still interpret the following control byte.
Wide characters wrap before column 80. Backspace and partial erasure remove
whole wide characters; scrolling copies both planes, filling the new row with
the current attribute. Buffered DOS input reserves pair capacity and edits a
complete character on Backspace. Committed single-byte/SJIS keyboard bytes can
reach the focused DOS slot; the Editor IME does not become a resident DOS FEP.

PC-98 attributes are display (bit 0), blink (1), reverse (2), underline (3),
normal-mode vertical line (4), blue/red/green (5/6/7). Blink is driven by the
24-bit ARTIC clock at a half-second interval, independent of keyboard activity.
The cursor follows the logical position, snaps to a wide lead and spans both
halves. Colour/effect changes alone damage the affected rows. Existing scene
clipping, occlusion, movement, minimization and focus routing remain in use.

Bounded ANSI CSI supports H/f, A/B/C/D/G, J/K, s/u and SGR 0, 30-37/39,
4/24, 5/25, 7/27, 8/28. Parameters saturate and excess parameters are consumed
without overflow. PC-98 text has no independent background plane; unrelated
SGR, semigraphics, downloadable fonts and arbitrary GDC hardware modes are
outside this stage. The legacy PM32 byte console retains its existing contract.

## Read-only snapshot boundary

Legacy K386Mailbox/K386View, K32Manager, GUI_SNAPSHOT v1 (2080 bytes), GUI
events and graphical-client ABI are unchanged. The former read-only reserved
word in v1 now reports the backend's text revision; older readers ignore it.
GUI_SNAPSHOT v2 is an explicit 8032-byte, fixed 80x25 snapshot with version,
size, slot, generation, revision, state and cursor followed by two u16 planes.
It is available only to the dedicated desktop for its owned V86 generation.
Supervisor/read-only output, stale generations, foreign slots, PM32 slots,
bad versions and sizes are rejected. No arbitrary memory address is accepted.

The real-mode broker streams through existing 4096-byte scratch. The V86
transport (command 14, INT 2Fh query D38Ah) reuses the monitor's existing
1024-byte transfer buffer for a 992-byte record, at most three rows per read.
No guest runs while one synchronous GUI snapshot is assembled; slot/revision
checks prevent mixing producers. Failed snapshots are discarded by the CPL3
desktop. The public XMS 4096-byte transfer contract is unchanged.

No additional conventional or XMS allocation is introduced. Both resident
modules retain their 64 KiB allocation. Link guards reserve at least 8 KiB of
protected stack in K386 and about 7 KiB in KGUILDR, plus the existing 4 KiB
real-mode stack. Sentinels detect crossing those bounds during real execution;
GUI teardown publishes the result in debugger reserved[0]. The dedicated GUI
still owns 4 MiB, with two private 4 MiB application slots and 16 MiB settings.

## Verification and distribution

`make -C tools/k386 all test` includes ASan/UBSan tests for split/malformed SJIS,
whole-character editing, wrap/scroll, both planes, bounded CSI and attributes.
The GUI broker tests verify v1 compatibility and v2 ownership/generation/range
rejection. Image tests check actual ELF/MZ layout and stack headroom.
TERM98.COM performs real DOS output, direct private TVRAM changes, buffered
input, wrapping/scrolling and optional contained faults. webnp2 supplies paired
PEGC/Cirrus acceptance, exact CG-ROM pixels and colours, half clipping, blink,
cursor, peer isolation/progress, HDD/FD boot and complete cleanup/media invariance.

The GUI disk's K386.TXT is a concise offline guide to leave FD workspace room.
The separate base K386 disk retains the full original guide. No user image is
updated, converted, formatted or repaired by distribution generation.
