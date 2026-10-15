# KDESK graphics and clipboard update

This update adds a PC-9821 PEGC 256-color backend (with the existing 16-color
fallback), live window movement, text copy/cut/paste, and KVIEW/KPAINT image tools.
Images use uncompressed BMP on DOS; WebNP2 provides host text/image exchange and
conversion of browser-supported image files to the native BMP format.

The existing keyboard controls, foreground DOS launch/return, safe memo saves,
and protected existing files remain part of the regression checks. CI rebuilds
the DOS applications and exercises them in WebNP2 before updating the bundled disk.

Controls and hardware details: [window-desktop.md](window-desktop.md),
[image guide](../tools/window/KIMAGE.TXT). The image clipboard is process-local;
text parent clipboard and WebNP2 host exchange bridge foreground applications.
The clipboard/image panel asks the browser for permission only on explicit
read/copy actions. JPEG/PNG imports fit to 320x200 and quantize to the DOS palette.
