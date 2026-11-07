# PC-98 HDD recognition and basic I/O proof

Scope: boot the existing DOS 4.0 FD, register an opt-in additional PC-98 SASI
block device and verify capacity, filesystem reads/writes and cold-boot retention
on a newly generated disposable FAT16 HDD. Preserve the original DOS kernel and
FD boot path. Do not format or modify existing user disks.

First prove the NP2kai BIOS interface, geometry, PC-98 partition/BPB mapping,
physical/logical sector translation, partial errors and DMA/buffer boundaries.
The driver must reject unsupported media and out-of-partition transfers.
Use independent FAT inspection of the actual exported HDD and unchanged FD.

Acceptance: host policy/boundary tests and actual Playwright browser DOS I/O,
capacity matching the image, >64 KiB data, overwrite, restart/readback, two FAT
copies and allocation integrity, malformed/absent media, and existing regressions.
Both repositories pin the same native commit. Record evidence and limitations
in PRs and issue #31. KGUI32 HDD workspace routing, app migration and HDD-only
boot are later phases; this change must not claim them as implemented.
