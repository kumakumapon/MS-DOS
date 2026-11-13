# Japanese text: Shift-JIS and PC-98 CG-ROM

The graphical desktop, Editor, Files TXT preview and recovery preview support
printable ASCII, JIS X 0201 half-width kana and two-byte JIS X 0208 Shift-JIS
(leads 81–9F/E0–EF, trails 40–7E/80–FC). This stage reads and saves existing
Shift-JIS documents. Editor also provides the app-local IME described below.
It does not add Unicode conversion, CP932 extensions,
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
Use the document selection and Ctrl+C to copy a Japanese query, or compose one
directly in either Find/Replace field. There is no host clipboard bridge.

## Japanese input and word conversion

Input starts **Off**, preserving existing ASCII and half-width kana editing.
Use **F4** to toggle Off/Romaji, **F5** to choose Romaji/JIS kana, or **Ctrl+J**
to cycle Off/Romaji/Kana. Tools → IME exposes the same settings as buttons.
**F6** chooses hiragana and **F7** katakana, without implicitly committing.
Each Editor owns its mode, preedit, candidates and dictionary buffer.

Romaji supports Hepburn/Kunrei spellings, small `x`/`l` kana, contracted sounds,
foreign sounds, doubled consonants and `n`/`nn`/`n'`. For example `nihongo`
produces にほんご, `konnichiha` こんにちは and `gakkou` がっこう. An unfinished
syllable remains visible and cannot be silently dropped during conversion or
commit. `vu` uses ヴ because hiragana ゔ is outside standard Shift-JIS.

JIS kana follows the physical Japanese keyboard: `t` = か, `f` = は, `@` adds
dakuten and `[` handakuten. Shift+3/4/5/6/7/8/9 and Shift+E/Z produce small
vowels/ya/yu/yo/i/tsu. Repeating a voicing mark toggles it; changing the mark
switches voiced/semivoiced kana. BIOS half-width kana input is normalized into
full-width preedit. Shift+comma/period produce 、/。; `-` in Romaji gives ー.

The preedit panel is separate from the document. Left/Right/Home/End move on
whole-character boundaries; Backspace/Delete edit the reading. **Space** opens
word candidates; Space/Down and Shift+Space/Up cycle them. Page Up/Down and
Prev/Next change pages, Home/End choose the first/last, number keys and mouse
select a visible row. Selection never commits. **Enter/Commit** explicitly
inserts the selected result, or the current kana reading, as one undoable edit
replacing the existing selection. Esc first returns candidates to the reading,
then cancels preedit; Cancel and Ctrl+Z cancel it immediately without changing
document/history. A second Ctrl+Z operates on document history as usual.

Save/New/Open, mode changes, and ordinary close require commit or cancel first.
F10/close is canceled while preedit exists, and must be retried after resolving
it. Focus/resize preserve it. Explicit Force End and faults discard the app as
before. A 64 KiB document or 63-byte Find/Replace field overflow keeps the
composition and original text. Preedit itself is bounded to 96 Shift-JIS bytes.

`ime/words.tsv` is an original MIT-licensed small system word dictionary;
`ime-build.py` reproducibly generates `KGIME.DIC` and input tables offline.
Conversion is whole-reading **word lookup**, followed by deduplicated hiragana
and katakana fallbacks. It does not perform phrase segmentation, grammatical
inflection, prediction, learning or user-dictionary writes. Unknown readings
remain usable as kana. Broader vocabulary and phrase/learning support are
separate extensions, not prerequisites for Japanese text entry.

The root dictionary is read lazily through the existing read-only file API.
The handle always closes before parsing. The entire 32-byte versioned header,
lengths, entry counts, FNV checksum, reserved fields and every Shift-JIS string
are validated within a 32 KiB bound before adoption. Missing/corrupt dictionaries
show a notice and retain kana conversion and explicit commit/cancel. No new
syscall, privileged IME, input wire, write surface, slot or memory budget is added.

## Drawing boundary

The existing `KgDraw`/`KgRequest`/event wire sizes and calls 64–72 are unchanged.
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
ASan/UBSan IME tests cover nasal/consonant ambiguity, small/voiced kana, candidate
deduplication and paging, preedit cursor/bounds, malformed/truncated dictionaries,
one-step selection replacement/history and retention on a full document.

webnp2 supplies the linked PEGC/Cirrus E2E for actual CG-ROM glyph pixels,
Files/Editor, independent clipboard consumers, save/export/cold boot, invalid
input retention and complete cleanup on HDD-only and explicit FD boot paths.
Browser servers and E2E run only in GitHub Actions for this chat.
