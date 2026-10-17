OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0x100;
 .text : { *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 __bss_start = .;
 .bss : { *(.bss*) *(COMMON) }
 __bss_end = .;
 ASSERT(__bss_end < 0xc000, "K386 needs separate real/protected stacks")
 /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
