# V86 verified HDD reads

V86 accepts A: and the KHD98 volume identified through the actual DOS DPB and
resident driver record. A matching label/file on an unrelated volume is not
sufficient. The read-only broker never formats a disk or changes host DOS's
current drive or directories. Disk create/write/delete/rename and writable opens
remain denied; private CON/NUL and console output retain their existing behavior.

## Use and distribution

In webnp2's standalone HDD profile the boot root and verified HDD are A:.
Choose DOS Shell and use `CD \APPS` or `TYPE A:\WORK\NOTE.TXT`.
See [standalone HDD boot](../../ports/pc98/hdd/BOOT.md).
In the compatibility compact-FD + APPS-HDD profile, choose DOS Shell.
Use the displayed HDD letter (normally B:), for example `B:`, `CD \APPS`,
`DIR`, or `TYPE B:\WORK\NOTE.TXT`. A root DOS application can be launched with
an explicit verified HDD path. A missing bare FD-root program may resolve under
the verified HDD's APPS directory. A found but invalid/device/I/O-failed FD file
never falls back to HDD. The standard six shipped APPS remain unchanged; the
E2E suite adds DOS programs/data only to private copies of marked test volumes.

The explicit full-FD profile still works without HDD. The existing KDESK/386
FD profile remains an FD-only boot configuration; its shared broker can use the
HDD when a verified KHD98 driver is installed. Arbitrary HDD formats remain
outside this profile. No automatic upgrade of a user's saved disk occurs.

## DOS contracts

Each VM owns its default drive plus A:/HDD current directories. A new root starts
in its program's directory; nested EXEC shares that VM's drive/directories and
inherits owned read-only file handles. Child termination restores the parent's
CPU/PSP, DTA/search state and handles, while file cursors retain DOS duplication
semantics. Another VM's drive/directory state is independent.

Supported services include drive select/query (0Eh/19h), get-CWD/CD (47h/3Bh),
read-only open/read/close/seek/duplicate, attributes, owned FindFirst/FindNext,
and root/nested EXEC. Drive-relative, absolute and rooted paths are canonical
ASCII 8.3 paths. Devices, UNC, wildcards in directories, malformed/overlong paths
and drives outside A:/the verified HDD are rejected. Search continuation retains
its original verified drive and checks the owner's private DTA state.

This is a bounded DOS subset, not complete DOS/BIOS compatibility. Disk BIOS
access is not forwarded. V86 does not use the writable PM32 Work save/recovery
API. All disk services run at the serialized real-mode boundary with a temporary
fail-critical-error handler; INT24 is restored on every outcome.

## Memory and validation

The mailbox/session wire, two application slots, 0xc000 monitor stack boundary
and public 4096-byte XMS transfer contract remain unchanged. The private scratch
buffer is 1024 bytes; larger reads/transfers and environments are staged in
bounded pieces. Guest zero initialization seeds 32 bytes and copies only already
zeroed, disjoint XMS ranges, avoiding a resident 1 KiB zero table.

Native tests cover private paths/drives, devices, bounds, full public XMS
transfers, zeroing and failures. VDOS.EXE is an actual V86 MZ probe whose bounded
statistics are captured from the executing guest PSP. Paired webnp2 CI covers
70,001-byte HDD reads, >64 KiB seek, nested EXEC and 1501-byte environments,
inherited file cursor, private DTA, writable-attribute file write denial, FD
precedence, original COMMAND.COM on HDD, two V86 contexts, mixed PM32 progress,
forced/fault/normal cleanup and byte-identical FD/HDD on PEGC/Cirrus.

Browser E2E runs in GitHub Actions. Do not start npm run preview in this chat.
