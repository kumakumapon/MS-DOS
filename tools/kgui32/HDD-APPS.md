# HDD application distribution (stage 3)

Status: implementation and verification in progress; do not merge yet.

The coordinated webnp2 distribution keeps DOS, K386, K32HOST, KGUILDR and
KGUI32 on the boot FD. Move the four GUI clients and selected PM32 text
applications into APPS on a newly generated, verified KHD98 FAT16 HDD.
Retain the complete legacy FD as the explicit standalone compatibility choice.
No HDD IPL, V86 drive expansion, Files navigation or recovery UI is included.

Both the mixed manager's classification and the PM32 loader must use the same
bounded application open routine. Bare 8.3 names retain FD-root precedence,
then search APPS on a DOS-DPB-verified KHD98 drive only on missing FD files.
Explicit paths preserve existing behavior; character devices must never be read.
Argument parsing, package/ELF validation, slot ownership and cleanup remain intact.
No CWD or default-drive changes and no mounted-disk host writes are permitted.

Acceptance: native tests and both repository CI; generated provenance and hashes;
no relocated app copies on the distribution FD; actual PEGC/Cirrus launches,
calculator, clock, Files and Editor; HDD save/reopen/cold boot; PM32 arguments,
missing/corrupt app rejection and restart/cleanup; FD compatibility and unchanged
normal KDESK images. Keep PRs draft while any acceptance condition is incomplete.
Integrate MS-DOS first, verify main CI, then webnp2 (separate approval scope).
