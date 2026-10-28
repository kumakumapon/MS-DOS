OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 .text 0 : AT(0) { *(.start) *(.text*) }
 __resident_end = .;
 __bank_base = ALIGN(__resident_end,16);
 OVERLAY __bank_base : AT(0x10000) {
  .gui { *(.gui.text*) }
  .tools { *(.tools.text*) }
 }
 __gui_load = LOADADDR(.gui); __gui_size = SIZEOF(.gui);
 __tools_load = LOADADDR(.tools); __tools_size = SIZEOF(.tools);
 __code_end = __bank_base + MAX(SIZEOF(.gui),SIZEOF(.tools));
 __data_load = ALIGN(LOADADDR(.tools)+SIZEOF(.tools),16);
 __data_segment = __data_load / 16;
 .data 0x100 : AT(__data_load+0x100) { *(.rodata*) *(.data*) }
 __mz_image_end = LOADADDR(.data)+SIZEOF(.data);
 __bss_start = .;
 .bss (NOLOAD) : { *(.bss*) *(COMMON) }
 __bss_end = .;
 __stack_top = ALIGN(__bss_end+0x3000,16);
 __mz_cs = 0; __mz_ss = __data_segment; __mz_sp = __stack_top;
 __mz_memory_bytes = __data_load+__stack_top;
 __allocation_paras = 0x10+(__mz_memory_bytes / 16);
 ASSERT(__code_end <= 0x10000,"KDESK bank exceeds the 16-bit code window")
 ASSERT(__stack_top <= 0xfff0,"KDESK data must leave a 12 KiB stack")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
