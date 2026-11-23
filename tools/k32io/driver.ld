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
 __mz_cs = 0; __mz_ss = 0;
 /* Four PM32 slots share the original conventional-memory budget. */
 __mz_memory_bytes = 0x8000;
 __mz_sp = __mz_memory_bytes - 16;
 __resident_paragraphs = (__mz_memory_bytes + 0x100) / 16;
 __io_stack_guard = __mz_memory_bytes - 0x1800;
 ASSERT(__bss_end < __io_stack_guard,"I/O broker retains 6 KiB stack")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
