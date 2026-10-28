# KDESK/386 window applications

On the enhanced desktop, the launcher and root EDIT98/EDIT, CPANEL, KVIEW and
KPAINT EXE entries open guest GUI applications. FD98 opens the Files tools.
The standalone EXEs keep their original DOS interfaces. Other DOS applications
keep their existing execution paths; this is not automatic GUI conversion.

Each tool has one live window with its own task ID, input queue, geometry and
state. Move, resize, minimize, restore and task switching use the existing window
manager. Waiting for input never suspends the two DOS sessions. Native tools
cooperate in the host DOS process; they do not have separate protected address
spaces. Synchronous disk calls are serialized and can delay a desktop turn.

## Controls

- Text editor: open TXT through Files; arrows/Home/End and mouse position the
  cursor. Ctrl+S saves, Ctrl+W saves as, Ctrl+Z undoes/redoes, Ctrl+R asks before
  reloading/discarding, Ctrl+C copies the document, Ctrl+V pastes. The buffer is
  8 KiB (standalone EDIT98 retains 16 KiB); clipboard text is at most 1023 bytes.
  Shift_JIS files and CRLF saving use the existing editor core. Paths are ASCII
  DOS names. Use Files to open another document after saving or reloading.
- Settings: X/E toggle XMS/EMS, +/- change FILES, [/] change BUFFERS, F changes
  filer startup, S saves, R reloads. Click the corresponding setting or Save
  footer. Settings take effect after reboot. Save errors keep the dirty guard.
- Files: F opens actions; R rename, C copy, M move, N mkdir, D delete. Enter
  applies a destination or confirms deletion; Esc cancels. Existing destinations
  are never overwritten. The original filer core preserves metadata and rollback.
- Images: uncompressed 1/4/8/24-bit BMP up to 320x200; editing uses the system
  indexed palette even on RGB565 displays. P/L/R/F/I/E/S select pencil, line,
  rectangle, fill, pick, erase and selection. Drag in the canvas; +/- choose a
  palette index, click the palette to choose within its page. Ctrl+C/X copies/cuts
  a selection (or entire image), Ctrl+V pastes at the origin, Ctrl+Z undoes/redoes.
  Ctrl+S saves, Ctrl+W saves as, Ctrl+R asks before reload/discard. Arrows pan
  horizontally and PgUp/PgDn scroll vertically. New images are 224x144.
- Esc/title X closes a clean window. Unsaved work blocks closing and desktop exit;
  save or explicitly reload/discard first. F10 exits the entire desktop.

## Code banks

The resident trampoline and hardware/file services remain below `__bank_base`.
GUI and tool sections share the remaining code offsets, with distinct immutable
load copies above the first 64 KiB. They share the DOS-owned data/stack segment.
The resident trampoline copies the requested bank, invokes a packet dispatcher,
and restores the previous bank before using its return address. IRQ handlers,
copy routines and the trampoline never occupy replaceable code. The compiler's
near-call convention and DOS relocation contract remain unchanged. No CPU mode
switch, screen replacement or VM snapshot is used for these windows.

`banked.ld` bounds each bank to the real 16-bit code window and preserves a
12 KiB stack. `test_bank.py` inspects the actual ELF/MZ bounds, relocation and
resident entry points. `generate-art.py --check` verifies that the compact icon
commands reproduce the original rectangle artwork. Original drawing source and
MIT license remain in the repository.

`make -C tools/window all test` builds the shared cores and banked KDESK.
WebNP2's pinned image generator includes the same EXEs; its native-tools E2E
checks saved text/config/BMP bytes, mouse drawing and undo, dirty guards,
window operations and live DOS CPU progress, in PEGC and Cirrus modes.
