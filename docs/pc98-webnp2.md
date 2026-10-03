# MS-DOS 2.0 PC-98 / WebNP2 port

Work in progress: add a PC-98 IPL and OEM BIOS for the bundled MS-DOS 2.0
kernel and SYSINIT modules. Target: 77 cylinders, 2 heads, 8 sectors of 1024
bytes, FAT12 XDF floppy on kumakumapon/webnp2.

The IBM PC MS-DOS 4 image is not compatible with PC-98. This port must boot
the actual Microsoft kernel, rather than substitute FreeDOS(98).
