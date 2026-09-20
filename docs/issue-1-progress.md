# Issue #1 implementation checkpoint

Scope: build MS-DOS 4.0 with bundled DOS tools in DOSBox-X on Windows,
create a reproducible FAT12 boot image, and verify it in QEMU.

## Progress

- [x] Read issue #1 and inspect repository baseline (2d04cac).
- [x] Record pinned tools and restoration provenance.
- [x] Implement isolated source preparation and build automation.
- [x] Build and inspect logs and artifacts.
- [x] Implement and validate boot image generation.
- [x] Verify VER, DIR, and file write/read in QEMU.
- [x] Document clean reproduction (PR readiness is recorded below).

## Resume

Read this file, inspect git status and the branch PR, then continue the first
unchecked item. Keep source preparation separate from historical originals.
Record commands, evidence, failures, and remaining work here in each checkpoint.
Do not mark unexecuted validation as passing.

## Checkpoint 2026-09-20

Draft PR: https://github.com/kumakumapon/MS-DOS/pull/2 (created before implementation).
Tools implemented in tools/dos4: isolated preparation/build, deterministic FAT12
image writer, WSL QEMU boot probe, and image format tests (3 passing).
Restoration is pinned to chasonr f4a7323, with provenance and byte-preserving patch.
DOSBox-X 2026.08.31 downloaded and extracted into .work/dosbox.
User approved using existing WSL Ubuntu QEMU; Ubuntu-24.04 has QEMU 8.2.2.

Current execution: .work/build is compiling the complete root NMAKE target.
Initial attempt failed because our trial INCLUDE path omitted the C runtime;
SETENV now uses the restoration's TOOLS/BLD/INC and TOOLS/BLD/LIB paths.
First failure log: .work/build/first-build-failed.log (ignored local artifact).
Resume by checking .work/build/checkpoint.json and v4.0/src/BUILD.LOG.
If needed rerun the build stage (no prepare on an existing tree), then image.py
and verify.py under WSL. A second fresh preparation/build is still required.
No QEMU boot validation has passed yet; keep PR draft until it does.


## Final implementation checkpoint 2026-09-20

Both build directories completed successfully; each has all 62 CPY.BAT outputs.
Both pristine 1.44 MB images have identical SHA256:
f5e8ec0b90a9db0441539cc1afdece3a0c223b196cd8a9d4d99c7240587e64cd
All file hashes also match. Both QEMU boot probes passed VER, DIR, write/read.
The one-command PowerShell wrapper passed in Resume mode, including WSL path
translation and QEMU verification. All 3 image unit tests passed.

Final entry point: tools/dos4/run.ps1. User guide: docs/build-msdos4.md.
Versioned evidence: docs/validation-msdos4.md and docs/validation/dos4/.
Earlier checkpoint statements are historical; the outstanding validation there
is now complete. No sub-agents were used. Ready for final PR review/undrafting.
