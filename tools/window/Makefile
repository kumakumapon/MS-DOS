CC ?= gcc
BUILD ?= build
CFLAGS = -m16 -march=i386 -Os -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -Wall -Wextra -Werror -DDOS

all: $(BUILD)/WIN98.COM $(BUILD)/WIN98.TXT $(BUILD)/WINLIC.TXT
$(BUILD):
	mkdir -p $@
$(BUILD)/start.o: ../filer/start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/core.o: ../filer/core.c ../filer/filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/dos98.o: ../filer/dos.c ../filer/filer.h ../filer/sjis.h | $(BUILD)
	$(CC) $(CFLAGS) -DPC98 -c $< -o $@
$(BUILD)/window.o: window.c ../filer/filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/window.elf: $(BUILD)/window.o $(BUILD)/start.o $(BUILD)/core.o $(BUILD)/dos98.o ../filer/com.ld
	ld -m elf_i386 -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/WIN98.COM: $(BUILD)/window.elf
	objcopy -O binary $< $@
$(BUILD)/WIN98.TXT: WIN98.TXT | $(BUILD)
	python3 -c "from pathlib import Path; Path('$@').write_bytes(Path('$<').read_text().replace('\n', '\r\n').encode('ascii'))"
$(BUILD)/WINLIC.TXT: LICENSE | $(BUILD)
	cp $< $@
.PHONY: all
