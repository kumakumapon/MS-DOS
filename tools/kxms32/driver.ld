OUTPUT_FORMAT(elf32-i386)
ENTRY(strategy)
SECTIONS {
 . = 0;
 .text : { *(.header) *(.prefix) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) *(COMMON) }
 resident_end = .;
 ASSERT(resident_end < 0x4000,"bounded high XMS supplement")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
