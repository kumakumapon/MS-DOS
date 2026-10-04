OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0x100;
 .text : { *(.start) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 __bss_start = .;
 .bss : { *(.bss*) *(COMMON) }
 __bss_end = .;
 ASSERT(__bss_end < 0xd000, "Filer must leave at least 12KB of stack")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
