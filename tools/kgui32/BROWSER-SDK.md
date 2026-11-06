# KGUI32 SDK 2 and read-only Files

This milestone adds reusable client-side input, list, scrollbar, button and
dialog controls, application-local focus, opt-in pointer motion/capture, and
KGFILES.EXE. The Files app lists A: root entries and previews ASCII text in
the same canvas, leaving the second mixed slot available to Clock or DOS.
The two-slot, 4 MiB-per-app, dedicated-GUI and 16 MiB XMS budgets remain.

The native read-only file broker will expose versioned, bounded metadata and
first/next/close directory operations. Cursor/DTA state belongs to each task,
DOS DTA is always restored, and stale cursors and invalid user ranges must be
rejected. A new negotiated feature bit separates this from old application
contracts. No arbitrary path, write, rename, delete or application EXEC API is
added. DOS calls stay outside graphical callbacks and scheduler interrupts.

SDK 1 wire layouts and old calculator/clock EXEs remain valid. New controls
live in the protected client library. Pointer events and local Tab are opt-in;
motion is coalesced, and capture is tied to the actual generation/window token.
The launcher, taskbar, movement, resize, minimize, pause/resume, cleanup and
legacy KDESK fallback remain available. ASCII and one canvas per app remain.

Validation includes pure control/broker policies, real CPL3 file metadata and
contents, malformed/stale requests, mouse/keyboard focus and scrolling, clock
coexistence, faults/restart, resource reclamation and full FAT12 invariance on
PEGC and Cirrus. Linker stack/64 KiB boundaries must pass. Both repository CIs
must succeed before review; the PRs will not be merged automatically.
