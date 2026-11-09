# HDD workspace: broker, Editor and shared picker

This extends SDK4; it does not change the original DOS kernel or the KGUI32
execution model. GUI and clients still run as isolated CPL3 PM32 programs with
two 4 MiB application slots and a separate 4 MiB GUI allocation (16 MiB machine).

## Responsibilities

| Layer | Responsibility |
| --- | --- |
| KHD98.SYS | Existing real-mode DOS block driver, strict marked-volume recognition and bounded BIOS transfers. |
| K32HOST services-dos.c | Select a verified DOS drive through its DPB, provision WORK if absent, query DOS free space, serialize owned file operations. |
| workspace.c | Validate 8.3 TXT names/text/revisions; preflight temporary allocation; preserve TMP/BAK/rollback policy. |
| K32WorkInfo / sdk/k32x.h | Optional versioned operation 5; expose destination and capacity without accepting arbitrary paths. |
| picker.c / editor.c | Show the actual target, default Open to HDD Work, Save As and retain dirty text/history on failure. |
| WebNP2 generator / E2E | Ship a new FD+HDD pair, preserve user images, exercise guest DOS I/O and inspect exported FAT bytes. |

## Boot and compatibility

The WebNP2 generator appends `LASTDRIVE=E` and `DEVICE=KHD98.SYS` to the separate
GUI FD's CONFIG.SYS. The standard KDESK disk is unaffected. The driver normally
registers B:, but the broker scans DOS drives B: through Z: and verifies the
driver record's matching drive number, signature, version, readiness and exact
geometry. Labels or files on arbitrary disks do not authorize writes.

The companion THD has the same 1024-byte-sector FAT16 contract as the earlier
HDD proof: 41,435,136 data bytes, initially 41,426,944 free after HDD.ID and WORK.
The earlier proof volume is accepted; DOS creates WORK only when absent, without
altering existing files. A file named WORK or protected/unavailable directory is
an explicit HDD error. Unsupported/missing media leaves the FD path usable.
No generic HDD formatter, partition editor or HDD-only boot is introduced.

Save/Open take names only, and INFO has no target selector. Root browsing remains
read-only A:. The document limit is 64 KiB ASCII; each save uses temporary space
while retaining the original. Operations are synchronous at the real-mode DOS
boundary; clients resume after that bounded transfer. Host FAT editing must not
run while the guest has a mounted writable disk. Do not share a writable THD
between concurrently running machines.

## Build and verification

```sh
make -C ports/pc98/hdd all test
make -C tools/k38632 all test
make -C tools/kgui32 all test
```

WebNP2 pins this repository's exact commit in
`scripts/prepare-msdos4-cpanel-ci.sh`, builds normal -> K386 -> KGUI32 images and
records source/dirty/hash metadata. `scripts/verify-kgui32-hdd-workspace.mjs`
drives actual Editor windows via Playwright in PEGC/Cirrus, checks CPL3, distinct
CR3/physical allocations, full file bytes, both FAT copies, allocation ownership,
protected sectors/files, FD invariance, fresh-context cold boot and complete
handle/XMS/DOS/vector cleanup. It includes insufficient space, real DOS short
write after directory expansion, peer revision conflicts and actual INT 1Bh
failure using a dedicated disposable-image TSR. No host FAT mutation occurs
while the guest runs. See WebNP2's `docs/kgui32-hdd-workspace.md` for commands.

HDD application loading and the coordinated compact-FD distribution are described
in [HDD-APPS.md](HDD-APPS.md). Files Work browsing and Editor launch are described in [FILES-EDITOR.md](FILES-EDITOR.md).
Recovery UI, generic disks and HDD-only boot remain separate follow-up stages. Power-loss atomicity is not guaranteed;
export backups and follow [SAVE-SDK.md](SAVE-SDK.md)'s TMP/BAK recovery procedure.
