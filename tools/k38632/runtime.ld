OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0;
 .text : { *(.text*) }
 .data : { *(.data*) }
 __mz_image_end = .;
 .bss : { *(.bss*) }
 __bss_end = .;
 __mz_cs = 0; __mz_ss = 0; __mz_sp = 0xfff0;
 __mz_memory_bytes = 0x10000;
 ASSERT(. < 0xd000,"K32MEM must leave separate real/protected stacks")
 ASSERT(user_end-user_code <= 4096,"PM32 code must fit one page")
 /DISCARD/ : { *(.note*) *(.comment*) }
}
