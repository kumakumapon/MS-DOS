# Native DOS MZ executables

Build KDESK.EXE with separate code and data/stack segments, DOS relocation of the
data segment, an explicit stack allocation and PSP-aware child EXEC parameters.
Keep shared COM applications on their existing startup/linker path.

Build the resident K386 monitor as K386.EXE with an explicit DOS memory request.
Separate its PSP from its monitor segment when resizing, staying resident and
switching the DOS owner for broker calls. Preserve PM/V86 addressing and APIs.

Update the PC-98 image builder, guides and paired webnp2 importer/boot scripts.
Do not leave older same-name COM files shadowing EXEs in newly generated images.
Validate MZ headers, relocations, memory bounds and stack reservations, then boot
both profiles in WebNP2 and exercise child return, GUI apps and isolated V86 tasks.
Pin the tested native source in webnp2. Record CI and browser evidence in the PRs.

This changes the executable layout, not the scope of DOS/BIOS virtualization.
