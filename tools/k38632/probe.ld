OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0x100;
 .text : { *(.text*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) }
 __bss_end = .;
 ASSERT(. < 0xc000,"K32CPU must leave separate protected/real stacks")
 ASSERT(user_end-user_code <= 4096,"Ring-3 probe code must fit one page")
 /DISCARD/ : { *(.note*) *(.comment*) }
}
