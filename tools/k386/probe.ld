OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0x100;
 .text : { *(.text*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) }
 ASSERT(. < 0xd000, "K386 probe must leave 12 KiB of stack")
 /DISCARD/ : { *(.note*) *(.comment*) }
}
