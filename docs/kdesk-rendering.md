# KDESK 1.4: redraw and startup

WebNP2's PEGC backend now composes changes in the second 256 KiB VRAM page.
The visible page keeps the previous complete scene during painting. Completed
regions are copied to the visible page without first clearing it to the desktop
color. Keyboard selection and editing redraw only the active window. Opening,
closing, moving, resizing or switching windows composes the scene and presents
the union of damaged window rectangles, plus the status/taskbar rows. A full
presentation is used at startup and after returning from a DOS child.

Clock updates also use offscreen drawing. When another window overlaps the
clock widget, a composition preserves the upper window instead of painting
through it. The mouse pointer is hidden/restored around presentation. A drag
recomposes the backdrop without the active window, then uses the existing
pixel-resolution sprite against that backdrop. The back page is reused; no
full-screen conventional-memory framebuffer is required.

The 16-color backend retains direct drawing and outline moves. It does not
claim the PEGC buffering path. The COM linker still reserves 12 KiB of stack.
Printable ASCII glyphs are cached; extended CG-ROM glyphs (including half-width
kana in file names) are read on demand to retain them within the segment limit.

The WebNP2 image importer enables a single marked `IF EXIST KDESK.COM KDESK`
command in AUTOEXEC.BAT. F10 exits to a normal DOS prompt; it does not restart
the desktop. CPANEL preserves this block when changing its own settings.
The source-port validation disk keeps its normal DOS prompt for kernel tests.

The browser verifier samples actual displayed video frames during icon/list
selection, window switches and an overlapped clock update. The permanent menu
must never disappear into the desktop background. Existing pointer/drag,
window management, calculator/clock, image/clipboard and DOS-return checks run
alongside it. The bundled image also verifies automatic startup, F10 return,
CPANEL preservation, and startup again after reload.
