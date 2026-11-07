OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0x100;
 .text : { *(.text*) }
 resident_paras = (. + 15) / 16;
 /DISCARD/ : { *(.note*) *(.comment) }
}
