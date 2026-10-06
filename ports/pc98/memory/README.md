# PC-98 DOS 4.0 extended-memory drivers

Tracking issue: kumakumapon/MS-DOS#7.

The DOS 4.0 PC-98 disk image includes PC-98-aware XMS and EMS drivers. These
are upstream PC-98 drivers packaged with DOS 4.0, not a claim that the
IBM-specific DOS 4.0 EMM386 source has been ported.

## Drivers in the image

- `FDXMS286.SYS` provides XMS 2.0 using the PC-98 memory map. Its source and
  GPL-2.0 license are preserved in `upstream/fdxms286/`.
- `EMM386.EXE` is the NEC PC-98 build, version 2.26. Its source and license
  are preserved in `upstream/emm386-nec/`.
- `CONFIG.SYS` loads FDXMS286 first, then EMM386 with `EMM=8192`. The explicit
  EMS pool is required with this XMS manager for EMS page-handle allocation.
- `XMSCHK.COM` and `EMSCHK.COM` exercise allocation, data movement/page
  mapping, and release from the guest.

The prebuilt driver binaries are from the FreeDOS(98) boot disk distributed
by WebNP2 at the time of packaging. Their SHA-256 hashes are:

| File | SHA-256 |
| --- | --- |
| `bin/FDXMS286.SYS` | `cd15149999373e79ed2269a2dbbaee61ea7226f8330fad6f60309606e8fd6c5f` |
| `bin/EMM386.EXE` | `8054f03bc762e22ba9897c901cbf24bd7f406449b7cb390a685fa3d828a90ef0` |

Public upstream source snapshots are vendored with their licenses:

- FDXMS286: `FDOS/himem`, commit
  `25aca5c4bec2fc4197c9b0f71c6d7dd1c331e96f` (GPL-2.0).
- NEC EMM386: `lpproj/emm386.nec`, commit
  `e3bfad17d85a549876f08c7ddb656302a4541db7` (Artistic License).

The EMM386 source requires OpenWatcom and Borland Turbo Assembler to rebuild;
the binary is kept byte-for-byte from the cited FreeDOS(98) distribution. The
FDXMS286 binary identifies itself as `0.03.Temperaments.r1`; the public source
snapshot identifies the base `0.03.Temperaments` version. `dos4.py` packages
the prebuilt PC-98 binary unchanged and does not claim it can be reproduced
from that base source snapshot.

## Verified behavior

In WebNP2, the source-built DOS 4.0 image boots with both drivers at `mem=1`
and `mem=13`. The bundled checks pass XMS allocation, move to and from
conventional memory, and release; EMS allocation, logical-page mapping,
read/write verification, and release. The 13 MiB run also confirms that XMS
memory remains available while the EMS pool is enabled.

UMB allocation is not supported by this image. The PC-98 EMM386 build reports
that no suitable UMB block is available in the tested WebNP2 memory map; the
image does not request `DOS=UMB` or claim UMB support. The original IBM-targeted
DOS 4.0 EMM386 build remains unported and is not used by this image.
