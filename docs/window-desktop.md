# Kuma Desktop for PC-98

Kuma Desktop (`KDESK.COM`) provides an original window desktop for the MS-DOS 4.0 PC-98 port
on WebNP2. The desktop renders 640 x 400 graphics with a 256-color PC-9821 PEGC
backend and CG-ROM glyphs. Four-plane 16-color rendering is available as a fallback.
It requires a 386-compatible CPU, like the
existing DOS utilities.

The desktop provides overlapping launcher, files, memo, help and about
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

F1 opens help, F2 the launcher, F3 files, F4 memo, and F5 about. Tab switches
windows, F6 followed by arrows moves the active window, F7 followed by arrows
resizes it, and Enter finishes either operation. F8 minimizes; Tab restores.
F9 or the Paint menu starts KPAINT.
Escape closes the active window, and F10 exits to DOS. The memo prompts before
discarding unsaved changes. A PC-98 bus mouse works without installing a mouse driver.

Click the top menus to open windows and click a window to focus it. Drag its
title bar to move, or its lower-right grip to resize. Moves show the full window bitmap at pixel resolution using a background
snapshot in the second PEGC VRAM page and a DOS-allocated sprite. Resizing uses
an outline; low-memory/16-color movement also falls back to an outline. The
window stays within the desktop bounds. The title buttons
minimize (`_`) and close (`x`); taskbar buttons restore and focus windows.
Launcher rows start programs with one click. Files supports row selection,
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
