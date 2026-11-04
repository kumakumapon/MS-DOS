# KGUI32: independent protected-mode desktop, first milestone

Baseline: MS-DOS main 1d5df51d27cd1c0801f014729fb547f89fb82a35 and WebNP2
master 904bfa63ba84777dabfe048e3e1c7dedba62cfcd. This plan precedes functional
implementation; it is not a completion claim.

Build an original 32-bit GUI without starting KDESK. The PM32 program owns
composition, window geometry, hit testing, mouse cursor and input handling.
Trusted PC-98 drivers transfer bounded damaged rectangles and deliver input at
real-mode service boundaries. DOS services remain serialized outside IRQs.
Keep the original kernel, KDESK fallback, licences and PM32/V86 app APIs.

Give the desktop a dedicated execution context rather than consuming either
of the two shared application slots. First prove desktop boot, real CPL3/PE/PG,
private backing, keyboard/mouse input, smooth window drag and clean DOS return.
Validate PEGC 640x400 indexed colour, then Cirrus 800x600 RGB565, plus a
transactional low-memory refusal and owned resource cleanup. Inspect actual
CPU frames, RAM, framebuffer pixels, input events and whole FAT12 hashes in
WebNP2/Playwright. Retain all existing tests and CI.

The first release is a desktop foundation, not a full replacement environment:
DOS/application windows, general widget SDK and graphical application ports are
later milestones. An isolated execution/ownership proof must show that the
dedicated GUI context leaves the two application contexts available. Document
unimplemented items explicitly; never mark broken code complete.

Publish a separate optional image and build it from a pinned native SHA. Do not
commit generated images/manifests/READMEs, change saved disks, or merge PRs.
Open draft PRs before implementation. Keep them draft until first-milestone
acceptance and latest CI pass; record technical decisions and actual results.
