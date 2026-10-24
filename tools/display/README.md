# PC-9821 Cirrus RGB565 backend

KDESK can use an 800x600 RGB565 desktop when WebNP2 enables its emulated native
Cirrus adapter (`USE_CLGD=true`, `CLGDTYPE=90`, `WAB_ANSW=1`). This is guest-side
PC-98 I/O and VRAM, with no VGA BIOS or Windows code/resources. The backend is
original code; the register mapping and bank/BLT semantics follow NP2kai's
existing PC-9821 Cirrus implementation.

The initial supported adapter model is 5Ah, CL-GD5430/5434. The guest detects
both board/chip and writable independent VRAM at 0 and 2MiB before selecting
output. The WebNP2 model provides 4MiB: visible framebuffer at0, composition
at1MiB, cached wallpaper at2MiB. Unsupported devices retain PEGC/planar output.
This is verified against the emulator; no physical accelerator board is claimed
tested. `cirrus_open` also validates a 1024x768 mode, but KDESK integration and
acceptance currently target 800x600 only.

- Banked 16-bit pixel I/O uses 32KiB windows and 16KiB bank units.
- Hardware source-copy BLTs transfer damage/composition rectangles; bounded
  polling and CPU copy fallbacks handle an unavailable/busy blitter.
- Window layout, taskbar, scrolling bounds and mouse use the desktop dimensions.
  DOS session contents remain private 80x25 text screens inside these windows.
- Cursor save/restore and drag sprites preserve complete RGB565 pixels.
- A foreground DOS/image tool restores its normal video mode; KDESK reacquires
  enhanced video on return. Clipboard coordinates are clamped to each app's mode.
- On exit the driver restores changed registers, palette-DAC mode, VRAM window,
  relay and video-enable state, then the existing BIOS/PEGC restoration runs.

`make -C tools/display all` builds KDISP.COM, a real-DOS diagnostic (not a V86
app). It writes all65,536 RGB565 codes in a256x256 region, verifies selected
VRAM words by guest CPU reads and waits for a key before restoring video.
WebNP2's `verify-display-probe.mjs` asserts all65,536 distinct canvas colors,
exact primary colors/green low bit, DOS return and whole-image immutability.
`verify-kdesk-highcolor.mjs` exercises the native GUI, high-coordinate bus mouse,
independent V86 contexts and unchanged disk data.

Limitations: external DOS graphics virtualization is unchanged; this does not
provide simultaneous direct GDC/PEGC applications. Existing external image
editors keep their 256-color buffers. The new pixel format applies to KDESK's
desktop rendering and the native display driver. General DOS/BIOS compatibility
remains bounded as described in tools/k386/STATUS.md.
