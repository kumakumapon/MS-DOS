# Kuma Desktop for PC-98

Kuma Desktop (`KDESK.COM`) provides an original window desktop for the MS-DOS 4.0 PC-98 port
on WebNP2. The desktop uses the PC-98 640 x 400 graphics planes and native text
layer, and requires a 386-compatible CPU, like the existing DOS utilities.

The initial release provides overlapping launcher, files, memo, help and about
windows. Keyboard controls manage focus, movement, resizing and minimization.
DOS programs run in the foreground and return to the desktop when they exit.
This is a DOS application shell with built-in windows; external DOS programs
run one at a time.

Build with `make -C tools/window all`, or use `ports/pc98/dos4.py` to include
`KDESK.COM`, `KDESK.TXT` and `KDLIC.TXT` in the boot image. Start with `KDESK`
at the DOS prompt. Launch it from the directory containing the bundled utilities.

F1 opens help, F2 the launcher, F3 files, F4 memo, and F5 about. Tab switches
windows, F6 followed by arrows moves the active window, F7 followed by arrows
resizes it, and Enter finishes either operation. F8 minimizes; Tab restores.
Escape closes the active window, and F10 exits to DOS. The memo prompts before
discarding unsaved changes. This release uses keyboard input without requiring
a mouse driver.

The file window opens directories and launches COM/EXE programs, TXT files in
the editor, and BAS files in the installed BASIC. The launcher locates the
file manager, editor, settings panel and DOS shell relative to the starting
directory. Child programs use DOS EXEC; their exit returns to the desktop.
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

The browser verifier checks real graphics VRAM, move/resize, overlap and focus,
minimize/restore, directory navigation, memo save/cancel/discard, preservation
after a failed save, child COM/CPANEL/editor launch and return, exit to DOS and
memo persistence after browser reload. CI stores screenshots and results in
the `msdos4-pc98` artifact under `tools/window/build/pc98-validation`.
