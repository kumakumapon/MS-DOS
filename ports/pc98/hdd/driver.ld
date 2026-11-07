OUTPUT_FORMAT(elf32-i386)
ENTRY(strategy)
SECTIONS {
 . = 0;
 .text : { *(.header) *(.record) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) *(COMMON) }
 resident_end = .;
 ASSERT(khd_record == 18, "DOS header and diagnostic record layout")
 ASSERT(resident_end < 0x8000, "HDD driver must fit half a segment")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
