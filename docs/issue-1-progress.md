# Issue #1 implementation checkpoint

Scope: build MS-DOS 4.0 with bundled DOS tools in DOSBox-X on Windows,
create a reproducible FAT12 boot image, and verify it in QEMU.

## Progress

- [x] Read issue #1 and inspect repository baseline (2d04cac).
- [ ] Record pinned tools and restoration provenance.
- [ ] Implement isolated source preparation and build automation.
- [ ] Build and inspect logs and artifacts.
- [ ] Implement and validate boot image generation.
- [ ] Verify VER, DIR, and file write/read in QEMU.
- [ ] Document clean reproduction and mark PR ready.

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
