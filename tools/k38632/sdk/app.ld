OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
 . = 0x800000;
 .text : { *(.text.start) *(.text*) *(.rodata*) } :text
 . = ALIGN(4096);
 .data : { *(.data*) } :data
 .bss : { *(.bss*) *(COMMON); . += 4; } :data
 ASSERT(. <= 0xbfd000,"PM32 application exceeds fixed physical budget")
 /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) }
}
