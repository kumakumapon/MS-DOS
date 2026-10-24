# KDESK high resolution and RGB565 display

First target: 800x600 RGB565 using NP2kai's existing PC-98 Cirrus/WAB device.
Retain the 640x400 PEGC/planar backend and safe DOS return. Validate guest I/O,
VRAM and actual browser framebuffer before enabling the desktop backend.

Implementation gates: Cirrus detection/mode probe; WASM WAB surface transfer;
16-bit guest drawing and backing buffers; dynamic desktop geometry/mouse/input;
DOS session/clipboard/image/GUI regression; immutable image and source pins.
Until hardware execution is verified, the new mode remains opt-in/experimental.
