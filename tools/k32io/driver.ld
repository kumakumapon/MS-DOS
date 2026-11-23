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
 __mz_cs = 0; __mz_ss = 0; __mz_sp = 0x93f0;
 __mz_memory_bytes = 0x9400;
 ASSERT(__bss_end < 0x5400,"37 KiB I/O broker retains 16 KiB stack")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
