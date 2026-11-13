OUTPUT_FORMAT(elf32-i386)
ENTRY(capture_strategy)
SECTIONS { . = 0; .text : { *(.text*) } /DISCARD/ : { *(.note*) *(.comment*) } }
