OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0;
 .text : { *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 __mz_image_end = .;
 __bss_start = .;
 .bss : { *(.bss*) *(COMMON) }
 __bss_end = .;
 __mz_cs = 0; __mz_ss = 0; __mz_sp = 0x5ff0;
 __mz_memory_bytes = 0x6000;
 ASSERT(__bss_end < 0x4000,"24 KiB process broker retains 8 KiB checked stack")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
