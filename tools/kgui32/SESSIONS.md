# KGUI32 text sessions: implementation plan

Extend the published standalone CPL3 desktop using the existing two PM32/V86
slots. Add a validated, generation-bound session broker; desktop-owned windows
display canonical 80x25 text and forward keys only to their focused owner.
Support create/end/stop/resume/restart without replacing unrelated owners.
Keep GUI 4 MiB, PM32 4 MiB, 16 MiB configuration and read-only disk services.
Keep the legacy desktop, diagnostic modes and CPU engine compatible.

First acceptance: KSH+KSH, KSH+COUNT32 and VIEW32+COUNT32. Inspect real frames,
generations, private CR3/backing, text, cursor, keyboard routing and background
progress, including hidden/stopped/resumed/failed sessions. Test stale tokens,
failed loads, low memory, ownership cleanup and whole FAT12 preservation on
PEGC and Cirrus in WebNP2. General graphical client SDK, Japanese rendering,
clipboard, shared disk writes and slot expansion remain later milestones.

Open draft PRs before implementation. Keep them draft while acceptance or CI
is incomplete. Do not merge. Synchronize the WebNP2 native pin and image build.
