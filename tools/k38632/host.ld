OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0;
 .text : { *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 .task : { __task_start = .; *(.task.data) __task_end = .; }
 __mz_image_end = .;
 __bss_start = .;
 .bss : { *(.bss*) *(COMMON) }
 __bss_end = .;
 __mz_cs = 0; __mz_ss = 0; __mz_sp = 0xfff0;
 __mz_memory_bytes = 0x10000;
 ASSERT(__task_end-__task_start <= 8192,"PM32 task snapshot exceeds seed")
 ASSERT(__bss_end < 0xd000,"K32HOST must leave real/protected stacks")
 /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame) }
}
