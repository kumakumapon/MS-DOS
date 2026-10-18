# K386 DOS execution continuation

The fresh DOS view currently contains NUL bytes until its first program starts.
The desktop glyph renderer turns control characters into question marks. Clear
new views and provide a usable guest command prompt instead of rendering NULs.

This development branch extends the existing guest monitor with an original
KSH command shell, bounded COM/MZ loading and command tails, one child EXEC per
session with parent context restoration, read-only directory and current-directory
services, and virtual PUSHF/POPF/IRET plus user software interrupt handling.

The existing DOS kernel and emulator core remain unchanged. Both sessions keep
private RAM/screens/input. Disk writes remain denied. General COMMAND.COM/BIOS,
large or overlaid executables, TSRs and full virtual hardware IRQ delivery are
not assumed to work; actual compatibility and acceptance must be tested.

The two initial draft PRs precede implementation. WebNP2 must pin the tested
native commit and build both profiles. Validation must reproduce the initial
question marks, inspect actual MZ relocation and CPU/stack restoration, run both
guest prompts concurrently, and check exported disk hashes and existing GUI.
