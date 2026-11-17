# KDESK graphics and clipboard update

Canonical native guide. Browser-specific setup and integration: [webnp2](https://github.com/kumakumapon/webnp2/blob/master/docs/kdesk-media.md).

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

## DOS image tools

`KVIEW [file.bmp]` displays images (default: `COLORS.BMP`). `KPAINT [file.bmp]`
edits images, or creates a 224x144 document without a filename. Both accept
uncompressed 1/4/8/24-bit BMPs, including top-down files, up to 320x200 pixels.
Colors are mapped to the fixed desktop palette. Save writes 8-bit BMP.

| Key | Action |
| --- | --- |
| F1 / F2 | Help / Open |
| F3 / Ctrl+S / F4 | Save / Save / Save As (KPAINT) |
| F5 / F10 | New / Quit |
| P / L / R / F / I / E / S | Pencil / Line / Rectangle / Fill / Pick / Erase / Select |
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / Cut / Paste at the image cursor |
| Ctrl+Z / U | Swap the last edit for undo/redo |
| Arrows / Space | Move cursor / paint a pixel |

Drag on the image to draw/select; click a palette swatch to change color.
Right-click picks a canvas color. Copy uses the selected rectangle or whole
image. Image clipboard and one-level undo live in that process's DOS memory.
The complete controls are also included as `KIMAGE.TXT` on the disk.

Open/New/Quit prompt before discarding edits. Save As prompts before replacing
a different existing file. Save writes a temporary file and renames only after
successful close; failed writes preserve the original. `KDIMG.TMP`/`KDIMG.BAK`
are reserved for safe replacement and existing copies block saving.
