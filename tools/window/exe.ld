OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 /* Near code offsets and near data offsets have independent 64 KiB ranges. */
 .text 0 : AT(0) { *(.start) *(.text*) }
 __code_end = .;
 __data_load = ALIGN(__code_end, 16);
 __data_segment = __data_load / 16;
 .data 0x100 : AT(__data_load + 0x100) { *(.rodata*) *(.data*) }
 __mz_image_end = LOADADDR(.data) + SIZEOF(.data);
 __bss_start = .;
 .bss (NOLOAD) : { *(.bss*) *(COMMON) }
 __bss_end = .;
 __stack_top = ALIGN(__bss_end + 0x3000, 16);
 __mz_cs = 0;
 __mz_ss = __data_segment;
 __mz_sp = __stack_top;
 __mz_memory_bytes = __data_load + __stack_top;
 __allocation_paras = 0x10 + (__mz_memory_bytes / 16);
 ASSERT(__code_end <= 0x10000, "KDESK code exceeds one segment")
 ASSERT(__stack_top <= 0xfff0, "KDESK data must leave a 12 KiB stack")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
