OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0;
 .text : { *(.start) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 __mz_image_end = .;
 __bss_start = .;
 .bss (NOLOAD) : { *(.bss*) *(COMMON) }
 __bss_end = .;
 __mz_cs = 0; __mz_ss = 0; __mz_sp = 0xfff0;
 __mz_memory_bytes = 0x10000;
 ASSERT(!DEFINED(__kapp_sample) || __bss_end <= 0x800, "sample overlaps counter scratch")
 ASSERT(__bss_end < 0xcff0, "K386 application must leave 12 KiB of stack")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
