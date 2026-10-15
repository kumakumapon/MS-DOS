# KDESK graphics and clipboard update

This update adds a PC-9821 PEGC 256-color backend (with the existing 16-color
fallback), live window movement, text copy/cut/paste, and KVIEW/KPAINT image tools.
Images use uncompressed BMP on DOS; WebNP2 provides host text/image exchange and
conversion of browser-supported image files to the native BMP format.

The existing keyboard controls, foreground DOS launch/return, safe memo saves,
and protected existing files remain part of the regression checks. CI rebuilds
the DOS applications and exercises them in WebNP2 before updating the bundled disk.
