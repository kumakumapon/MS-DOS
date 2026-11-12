# Japanese text: Shift-JIS and PC-98 CG-ROM

The graphical desktop, Editor, Files TXT preview and recovery preview support
printable ASCII, JIS X 0201 half-width kana and two-byte JIS X 0208 Shift-JIS
(leads 81–9F/E0–EF, trails 40–7E/80–FC). This stage reads and saves existing
Shift-JIS documents. It does not add an IME, Unicode conversion, CP932 extensions,
Japanese 8.3 filenames, or full Japanese DOS terminal/character-attribute emulation.
English labels and the original two mixed execution slots remain.

Editor opens at most 65,536 raw bytes, validates the entire document before
adoption and normalizes CRLF to LF internally. Saving preserves Shift-JIS bytes
and the selected LF/CRLF format within the same 65,536-byte serialized bound.
The save broker validates lead/trail and CR/LF state across its 4096-byte copy
chunks before any filesystem mutation. Names, revisions, protected files,
full-disk errors and TMP/BAK handling retain their existing contracts.

Cursor/Shift selection, mouse hits, deletion, clipboard, Undo/Redo, wrapped
layout and 47-byte draw-command boundaries never split a two-byte character.
Search/Replace only begin at character boundaries: an ASCII-valued trail byte
cannot match as punctuation. Ctrl+V in a Find/Replace field accepts a complete,
single-line Shift-JIS guest clipboard value up to the 63-byte field bound.
Use the document selection and Ctrl+C to copy a Japanese query. There is no
host clipboard or Japanese composition input in this stage.

## Drawing boundary

The existing `GuiDraw`/`KgRequest`/event wire sizes and calls 64–72 are unchanged.
Text and button commands remain NUL-terminated arrays of 48 bytes; the last byte
is reserved for NUL. Half-width characters use 8×16 pixels and a pair uses 16×16.
Validation accounts for the encoded cell width before rendering. A clipped or
truncated run ends before the next complete character.

The desktop alone may call `GUI_FONT` (67) with the new 48-byte `GuiGlyph`:
u32 version=1, size=48, code (single byte or lead<<8|trail), columns=0, bitmap[32].
On success columns is 1 or 2; the bitmap is half-major, 16 rows per half. The
native loader validates the complete readable/writable user range, version,
size and code, then reads CG-ROM ports and resets the CG address/row registers.
It never writes glyph memory, returns a hardware pointer, or grants client I/O.
The original 1520-byte ASCII-table request remains valid. The CPL3 desktop caches
Japanese glyphs and composes clipped pixels into its existing framebuffer.

## Verification

Native tests cover malformed/truncated pairs, half-width kana, ASCII-valued
trail search, whole-character selection/replacement/history, draw clipping,
63/47/4096/65536-byte boundaries and rejection before filesystem mutation.
Desktop startup probes CG-ROM requests with actual CPL3 code, including invalid
codes, versions, supervisor memory and read-only output.

webnp2 supplies the linked PEGC/Cirrus E2E for actual CG-ROM glyph pixels,
Files/Editor, independent clipboard consumers, save/export/cold boot, invalid
input retention and complete cleanup on HDD-only and explicit FD boot paths.
Browser servers and E2E run only in GitHub Actions for this chat.
