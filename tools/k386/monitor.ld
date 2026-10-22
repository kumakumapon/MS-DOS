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
 __mz_cs = 0;
 __mz_ss = 0;
 __mz_sp = 0xfff0;
 __mz_memory_bytes = 0x10000;
 ASSERT(__bss_end < 0xc000, "K386 needs separate real/protected stacks")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
