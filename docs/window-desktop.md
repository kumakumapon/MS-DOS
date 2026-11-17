# Kuma Desktop for PC-98

Canonical native guide. Browser-specific setup and integration: [webnp2](https://github.com/kumakumapon/webnp2/blob/master/docs/window-desktop.md).

Kuma Desktop (`KDESK.COM`) provides an original window desktop for the MS-DOS 4.0 PC-98 port
on WebNP2. The desktop renders 640 x 400 graphics with a 256-color PC-9821 PEGC
backend and CG-ROM glyphs. Four-plane 16-color rendering is available as a fallback.
It requires a 386-compatible CPU, like the
existing DOS utilities.

The desktop provides overlapping launcher, files, memo, help, about, calculator and clock
windows. Mouse and keyboard controls manage focus, movement, resizing and minimization.
A teal desktop, light window bodies, blue focused titles, bevelled buttons,
shadows and a graphical pointer replace the original eight-color display.
DOS programs run in the foreground and return to the desktop when they exit.
This is a DOS application shell with built-in windows; external DOS programs
run one at a time.

Build with `make -C tools/window all`, or use `ports/pc98/dos4.py` to include
`KDESK.COM`, `KVIEW.COM`, `KPAINT.COM`, `COLORS.BMP`, their guides and
`KDLIC.TXT` in the boot image. Start with `KDESK`
at the DOS prompt. Launch it from the directory containing the bundled utilities.

On PEGC, KDESK composes changes in the second VRAM page, then copies completed
window regions to the visible page. Selection/editing refreshes the active
window, and focus/geometry changes restore damaged regions. The menu and
unaffected desktop remain visible throughout painting. See
[rendering and startup](kdesk-rendering.md).

F1 opens help, F2 the launcher, F3 files, F4 memo, and F5 about. Tab switches
windows, F6 followed by arrows moves the active window, F7 followed by arrows
resizes it, and Enter finishes either operation. F8 minimizes; Tab restores.
F9 or the Paint menu starts KPAINT.
Escape closes the active window, and F10 exits to DOS. The memo prompts before
discarding unsaved changes. A PC-98 bus mouse works without installing a mouse driver.

Click the top menus to open windows and click a window to focus it. Drag its
title bar to move, or its lower-right grip to resize. Moves show the full window bitmap at pixel resolution using a background
backdrop in the second PEGC VRAM page and a DOS-allocated sprite.
The backdrop is recomposed without the active window when a drag starts;
normal offscreen composition and live dragging share that page. Resizing uses
an outline; low-memory/16-color movement also falls back to an outline. The
window stays within the desktop bounds. The title buttons
minimize (`_`) and close (`x`); taskbar buttons restore and focus windows.
The launcher defaults to an icon grid: click to select, double-click or Open to
launch. V or List/Icons switches to the list with one-click launch. Arrows select
icons and the grid scrolls to keep the selection visible. Files supports row selection,
double-click or Open, Parent, and up/down scroll buttons. Memo Save and the
unsaved-changes Save/Discard/Cancel dialog are also clickable. In WebNP2,
move/click directly on the emulated screen; use Mouse resync if misaligned.

The file window opens directories and launches COM/EXE programs, TXT files in
the editor, BAS files in the installed BASIC, and BMP files in KVIEW. The launcher locates the
file manager, editor, settings panel and DOS shell relative to the starting
directory. Child programs use DOS EXEC; after their exit, press any key or
click to return to the desktop.
KVIEW/KPAINT inherit the desktop mouse position through an optional mailbox
extension in the parent PSP allocation and return the updated position on exit.
No resync is required between the desktop and its image tools. Other child
programs retain their own controls; KDESK mouse support does not add mouse
controls to the text editor or settings panel.
An unavailable program reports an error and keeps the desktop running.

The memo supports up to 1023 ASCII characters, CRLF disk files, and Ctrl+S
to save `NOTE.TXT` in the initial directory. It writes `KDTMP.TMP` before
moving the previous note to `KDOLD.TMP` and replacing it. Failed writes retain
the original note; existing temporary files prevent replacement. Non-ASCII or
larger existing notes remain available through the full text editor.

For a WebNP2 checkout with a test image at `public/test/dos4-window.xdf`:

```sh
CHROMIUM=/usr/bin/chromium WEBNP2_NO_SANDBOX=1 \
  node tools/window/verify-webnp2.mjs /path/to/webnp2 http://127.0.0.1:5173/
```

The browser verifier checks guest canvas output, pointer restoration and
edge clipping, mouse/keyboard move/resize, overlap and focus,
minimize/restore, directory navigation, memo save/cancel/discard, preservation
after a failed save, child COM/CPANEL/editor launch and mouse/keyboard return,
exit to DOS and
memo persistence after browser reload. CI stores screenshots and results in
the `msdos4-pc98` artifact under `tools/window/build/pc98-validation`.

See [calculator, clock and launcher controls](kdesk-utilities.md). Both utility
windows operate within the desktop; the clock keeps updating while other
built-in windows are active.

## PC-98 hardware

`hardware.c` polls the 8255 bus-mouse ports 7FD9h/7FDDh/7FDFh with HOLD
nibble reads, signed deltas and active-low buttons. It disables mouse IRQs only
while polling and restores the previous control value before DOS EXEC/exit.
The ARTIC counter times polling and double-clicks; the code does not depend on
a timer interrupt waking HLT. Input is collected during painting to avoid
clipped movement, lost button transitions and BIOS keyboard-buffer overflow.
The cursor saves/restores packed pixels or all four planes beneath it and clips
at screen edges.

The desktop selects PEGC packed pixels through 32 KiB banks and caches
8 x 16 CG-ROM glyphs. A non-PEGC machine retains the planar 16-color backend.
Labels are mirrored in hidden text VRAM for existing screen-text tooling.
Both palette banks, PEGC control/bank registers, display mode and visible text
are restored for child programs
and DOS. No external font or mouse TSR is embedded.

## Clipboard and image tools (1.2)

Memo supports Ctrl+A/C/X/V and Copy/Cut/Paste/All buttons. Copy/Cut use the entire
memo; Paste replaces its selected text or appends. The 1023-character ASCII limit
is checked before any editing. The clipboard remains in parent memory across
foreground child EXECs and is invalidated on exit.

KVIEW displays uncompressed 1/4/8/24-bit BMP, including top-down files, up to
320 x 200 pixels. KPAINT adds pencil/line/rectangle/fill/pick/erase/selection,
image copy/cut/paste, undo/redo and uncompressed 8-bit BMP save. It asks before
discarding changes or overwriting a different image; failed writes keep the
in-memory document and original file. KDIMG.TMP/KDIMG.BAK prevent partial
replacement. KIMAGE.TXT documents the controls; COLORS.BMP is generated from an
original color chart. All image tools share KDLIC.TXT.

WebNP2 provides an explicit media panel for host text/image clipboard exchange,
PNG export, and conversion of browser-supported images to palette BMP on FDD1.
It uses the versioned KDESKCLP mailbox in each live application's conventional
memory block. Text requests publish payload/length before command; the guest
acknowledges rejection without modifying its note. Image exports validate all
sizes and segment ranges. No host clipboard access occurs automatically.

The 256-entry palette reserves 16 UI colors, a 216-color RGB cube, and 24 desktop
gradient shades. The browser's RGB565 output may quantize channel values. The
verifier samples actual canvas pixels to prove more than 200 simultaneous RGB
colors, and tests a held-button live move, text clipboard, image edits, malformed
BMP rejection, save errors, overwrite cancellation and reloading.

## Current desktop input reference

### Mouse controls

Move/click directly on the emulated screen. The first click waits for mouse
synchronization; use the toolbar's Mouse resync control if the positions drift.
Click Apps/Files/Memo/Help/About to open a window, and click a window to focus it.

| Mouse action | Result |
| --- | --- |
| Drag title bar | Move the full window at pixel resolution while held |
| Drag lower-right grip | Preview size, then resize on release |
| `_` / `+` / `=` / `x` title button | Minimize / maximize / restore / close |
| Double-click title | Maximize or restore the exact previous position and size |
| Taskbar button | Focus/restore; clicking the active task minimizes it |
| Launcher icon | Select; double-click or Open starts it |
| Launcher List/Icons (V) | Toggle icon grid/list; list rows start with one click |
| File icon or list row / double-click | Select / open directory or file |
| Files Parent / Open / List or Icons | Parent directory / open / change view |
| Files FD98 (F) | Open the two-pane filer in this directory |
| Scrollbar arrows / track / thumb drag | Scroll a row / page / to a position |
| Memo Save / Copy / Cut / Paste / All | Save NOTE.TXT / use the text clipboard |
| Save / Discard / Cancel dialog | Resolve unsaved changes |

A navy gradient desktop, white content surfaces, blue focused titles with heavier
CG-ROM lettering, soft button edges and gold selection accents
make the controls easier to distinguish. The guest draws its own pointer,
including over text. A mouse driver is not required; keyboard controls also work.

| Key | Action |
| --- | --- |
| F1 / F2 / F3 / F4 / F5 | Help / Launcher / Files / Memo / About |
| Tab | Switch windows; restore a minimized window |
| Ctrl+G | Maximize/restore the active window |
| F6, then arrows, then Enter | Move the active window |
| F7, then arrows, then Enter | Resize the active window |
| F8 | Minimize the active window |
| F9 / Paint menu | Start the image editor |
| Escape | Close the active window |
| F10 | Exit to DOS |

The launcher opens the built-in icon file manager, text editor, settings panel, installed
BASIC, DOS shell, image viewer, image editor, calculator or clock.
Files, Calculator and Clock are built-in windows that work alongside Memo. See the
[utility and icon launcher guide](kdesk-utilities.md). KVIEW/KPAINT inherit and return the desktop mouse position, so switching
between them and KDESK needs no mouse resync. External DOS programs run one at
a time in the foreground.
Exit the program (use `EXIT` for the shell), then press any key or click to return to the
desktop. Each DOS tool retains its own controls (usually keyboard input). The Files window navigates directories and opens COM/EXE programs,
TXT files in the text editor, BAS files in the installed BASIC, and BMP files in KVIEW.

Files defaults to file-type icons: click selects, double-click/Enter opens, and
Backspace/Parent goes up. V or List/Icons toggles the list view. Arrows select
items, Page Up/Down moves a page, and Home/End selects an end. F or FD98 opens
the existing filer for copy, move, rename, delete and directory creation; the
button is hidden in narrow windows but F still works.

Launcher, Files, Memo, Help and About show a vertical scrollbar when content
overflows. Each window keeps its own scroll position. Icons reflow and text
wraps as you resize; Calculator and Clock fit their minimum window sizes.
Memo/Help/About also accept arrows, Page Up/Down and Home/End for scrolling.
Memo editing reveals the final line again and scrolling never changes or saves
text. See [scrollbar and file icon details](kdesk-explorer.md).

The built-in memo accepts 1023 ASCII characters. Ctrl+S saves `NOTE.TXT` with
CRLF line endings in the starting directory. Closing or exiting with unsaved
text offers save, discard, and cancel. Failed writes retain the original note;
existing `KDTMP.TMP` or `KDOLD.TMP` prevents replacement. Use the text editor
for Japanese or larger files. Ctrl+A selects the entire memo, Ctrl+C/Ctrl+X
copy/cut it, and Ctrl+V replaces the selection or appends text. Clipboard text
remains available while a foreground DOS program runs.
