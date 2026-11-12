# PC-98 DOS4 HDD recognition and basic I/O proof

`KHD98.SYS` is an original, optional real-mode DOS block driver. Load it with
`DEVICE=KHD98.SYS` from the existing PC-98 DOS4 FD. DOS assigns the next drive
letter (B: in the proof image). The original BIOS, DOS kernel and NP2kai core
are unchanged. GNU binutils/GCC's i386 `-m16` support is required:

```sh
make -C ports/pc98/hdd all test
```

This first driver deliberately accepts only a **new, marked disposable proof
volume**. It neither formats nor repairs user disks. Unsupported or absent
media registers zero units; the FD remains usable. Only HDD unit 0 is supported;
the driver does not hook interrupts or implement removable-media changes.
Do not attach a proof disk to multiple running emulator instances for writing.

## On-disk contract

* T98 THD: 256-byte container header; 615 cylinders, 8 heads, 33 sectors/track,
  256 physical bytes/sector. Physical capacity: 162360 sectors.
* PC-98 partition table: one A1/91 entry, cylinder 1 through cylinder 614,
  name `KHDPROOF`; no other partitions. IPL1 and 55AA signature required.
* Partition: physical LBA 264, 40524 **1024-byte logical sectors**, FAT16,
  4 sectors/cluster, 1 reserved sector, 2 FATs of 20 sectors, 512 root entries.
  BPB OEM `KHDPRF01`, label `KHDPROOF`, type `FAT16`.
* 10116 data clusters: 41,435,136 bytes usable filesystem capacity before
  files are allocated. The marker file `HDD.ID` consumes one cluster.
* The last 3 logical sectors fall beyond complete data clusters. The final
  sector contains a diagnostic marker and is used for boundary/readback tests.

Why 1024 bytes? The current OEM FD initializes `SYSI_MAXSEC` to 1024. DOS4
`SYSCONF.ASM` rejects a newly installed driver's larger BPB sector size. The
existing WebNP2 2048-byte logical-sector HDD formatter therefore cannot be
used unchanged. This separate proof generator respects the kernel's contract.

## Transport and error handling

The driver uses PC-98 INT 1Bh AH=06h/05h, AL=00h (physical LBA in DL:CX,
byte count BX, buffer ES:BP). It checks BIOS sense geometry (or the matching
SASI sense code), partition/BPB markers and the last physical sector before
registering the drive. A private 2 KiB-aligned bounce buffer avoids 64 KiB DMA
boundaries; each request transfers one 1024-byte logical sector at a time.
Caller far pointers are normalized, including offsets crossing FFFFh.

Logical ranges, conventional memory bounds and counts are checked before I/O.
Errors return DOS status with the completed sector count, preserving unread
buffers. Write-with-verify reads back and compares each sector. DOS owns FAT
allocation and caching; no host FAT writer runs concurrently with the guest.

A 64-byte `KHDDIOP1` diagnostic record at driver header+18 exposes readiness,
geometry, actual completed reads/writes, rejected requests, BIOS errors and
partial requests (layout in `policy.h`). It is diagnostic, not an app API.

## Guest probe

`HDDTEST.EXE` requires the exact capacity and `HDD.ID` marker, then identifies
this driver through DOS's DPB before attempting diagnostics. Commands:

| Command | Action |
|---|---|
| `HDDTEST R` | Recognize drive, report capacity, read marker |
| `HDDTEST W` | Create/truncate reserved `IOPROOF.DAT`, write 131073 bytes |
| `HDDTEST V` | Verify W's pattern and EOF |
| `HDDTEST O` | Overwrite the test file with a different pattern |
| `HDDTEST T` | Verify O's pattern and EOF |
| `HDDTEST B` | Bounds, 64K crossing, write-verify, partial BIOS failure |
| `HDDTEST N` | Assert the proof drive was not registered |

**W/O overwrite the reserved test filename on this disposable volume.** B
writes the same last-sector bytes back for write-verify; other boundary cases
must issue no writes. Its transient INT 1Bh fault hook fails the second read
of a two-sector request and restores the original vector before returning.

WebNP2's `scripts/update-pc98-hdd-proof.py` builds an opt-in FD+HDD pair from
this commit and its existing base DOS FD. `scripts/verify-pc98-hdd.mjs` tests
real DOS file operations, independently parses both FATs and directory/file
contents from the downloaded THD, and boots a fresh browser context for
retention. See that repository's `docs/pc98-hdd-proof.md` for the exact commands.

KGUI32 now uses this marked data-volume contract for its workspace; see
[HDD-WORKSPACE.md](../../../tools/kgui32/HDD-WORKSPACE.md). App distribution is
described in [HDD-APPS.md](../../../tools/kgui32/HDD-APPS.md), and the separate
standalone HDD profile in [BOOT.md](BOOT.md). Generic existing disks and
concurrent writable sharing remain outside these profiles.

`HDDFLT.COM` is a test-only TSR for disposable browser fixtures. It chains INT1Bh
and denies exactly the first HDD physical write (AX=0500h, AH=60h/CF set), leaving
FD functions unaffected. The normal AUTOEXEC never executes it. The dedicated
HDD workspace E2E loads it only into a cloned FD to test real DOS error handling;
restart the machine afterwards rather than using that diagnostic boot for work.
