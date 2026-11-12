# PC-98 standalone HDD boot

The new profile boots a newly generated marked KHD98 THD with no floppy attached.
Its PC-98 IPL reads a separate 4096-byte system loader, validates the fixed
partition/BPB, finds IO.SYS and MSDOS.SYS in the FAT16 root, and accepts only
bounded payload sizes and exact contiguous allocation chains. Loader failures
halt with `H1` or `HDD BOOT ERROR` before executing a system payload. No IPL or
loader stage writes to the disk.

The HDD-specific OEM IO.SYS embeds the exact KHD98.SYS transport at a paragraph
boundary, including its entire private stack/BSS extent. At SASI unit-0 boot it
links that driver as the built-in DOS A: device. The FD OEM profile retains its
original built-in floppy driver. Do not load KHD98.SYS again in the HDD profile's
CONFIG.SYS: a second alias of the same writable filesystem is not supported.

The original DOS4 kernel and COMMAND.COM are retained. The new IO.SYS is built
from the pinned restored SYSINIT source and the native BIOS, using the bundled
DOS assembler/linker under DOSBox and pinned JWasm for the OEM module. Only the
message-index/message-include and SYSINIT steps are needed for this OEM-only
build; unrelated DOS utilities are not rebuilt by web distribution jobs.

```sh
python3 ports/pc98/toolchain.py
python3 ports/pc98/hdd/oem.py --dosbox /usr/bin/dosbox \
  --jwasm .work/pc98-tools/JWasm/GccUnixR/jwasm
make -C ports/pc98/hdd all test
```

The source history must include tools/dos4/build.py's immutable BASE commit.
The webnp2 preparation script fetches that source explicitly when its clone is
shallow. Outputs remain under ignored build/work directories.

`boot_image.compose()` takes a newly generated marked APPS/Work volume and
returns a separate copy with IPL, loader and root system files. It rejects
unsupported geometry/BPB, disagreeing FATs, duplicate/device names, oversized
payloads and unavailable contiguous system allocation. It never mounts a disk
or updates a user's existing saved volume. The paired FD+data HDD and complete
standalone FD remain separate compatibility choices; existing data HDDs are not
automatically made bootable.

The real DOS DPB and active KHD98 record authorize A: as an HDD. PM32 Work then
uses A:\WORK with the HDD flag; bare missing root application names can resolve
under A:\APPS. V86 retains read-only disk services, private CWD and owned handles,
including when its verified HDD and boot root are both A:. Other drive letters
are not implicitly authorized.

Paired webnp2 acceptance covers PEGC/Cirrus HDD-only desktop/APPS, Work save and
cold boot, real-DOS transport bounds/64K/partial failure, V86 EXEC/ownership and
write denial, invalid boot media and standalone FD regression. Inspect actual
CPU records and exported media, and require complete GUI/DOS/XMS/handle/vector
cleanup. Browser tests run in GitHub Actions; do not start a server in this chat.
