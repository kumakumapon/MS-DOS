# KGUI32 graphical clients: implementation plan

Keep the existing two mixed V86/PM32 slots and the dedicated CPL3 GUI.
Add a versioned, bounded graphical-client protocol to the resident PM32 host,
with per-generation ownership, retained drawing commands and input queues.
The desktop remains the only compositor and hardware driver client; apps
have private address spaces and receive no VRAM, I/O or server pointers.

Build separate calculator and clock EXEs with a small C SDK and add a minimal
launcher to KGUI32. Preserve text windows, fallback KDESK, 16 MiB, PM32 4 MiB
budgets, read-only disks, existing ABI and licences. GUI+GUI and GUI+DOS must
continue running in the background. Stop/resume/end/fault and GUI exit must
reclaim only matching owners, including stale-window/token rejection.

Acceptance: calculator 6*7=42 by keys and mouse; independent clock progress;
focus/mouse/resize/minimize/restore; DOS input and background progress;
real CPU/CR3/backing and private RAM; invalid command/range/owner/size and
unavailable-server rejection; contained client fault and exact restart;
DOS/XMS/drawing reclamation and full FAT12 hashes. Run unit tests and real
WebNP2 Playwright on PEGC and Cirrus. Preserve existing suites, pin a clean
native commit, document limitations. Keep draft until acceptance and CI pass;
do not merge. Text input widgets, Japanese, clipboard and disk writes are later.
