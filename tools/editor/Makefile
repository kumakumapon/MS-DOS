CC ?= gcc
BUILD ?= build
CFLAGS = -m16 -march=i386 -Os -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -Wall -Wextra -Werror -DDOS
all: $(BUILD)/EDIT98.COM $(BUILD)/EDIT.COM $(BUILD)/EDIT.TXT $(BUILD)/EDITLIC.TXT $(BUILD)/JPHELLO.TXT
$(BUILD):
	mkdir -p $@
$(BUILD)/start.o: ../filer/start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/core.o: ../filer/core.c ../filer/filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/dos98.o: ../filer/dos.c ../filer/filer.h ../filer/sjis.h | $(BUILD)
	$(CC) $(CFLAGS) -DPC98 -c $< -o $@
$(BUILD)/editor.o: editor.c editor.h ../filer/sjis.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/ui.o: ui.c editor.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/edit.elf: $(BUILD)/editor.o $(BUILD)/ui.o $(BUILD)/start.o $(BUILD)/core.o $(BUILD)/dos98.o ../filer/com.ld
	ld -m elf_i386 -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/EDIT98.COM: $(BUILD)/edit.elf
	objcopy -O binary $< $@
$(BUILD)/EDIT.COM: $(BUILD)/EDIT98.COM
	cp $< $@
$(BUILD)/EDIT.TXT: EDIT.TXT | $(BUILD)
	python3 -c "from pathlib import Path; Path('$@').write_bytes(Path('$<').read_text().replace('\n', '\r\n').encode('ascii'))"
$(BUILD)/EDITLIC.TXT: LICENSE | $(BUILD)
	cp $< $@
$(BUILD)/JPHELLO.TXT: JPHELLO.UTF8 | $(BUILD)
	python3 -c "from pathlib import Path; Path('$@').write_bytes(Path('$<').read_text().replace('\n', '\r\n').encode('shift_jis'))"
$(BUILD)/editor-host: editor.c host_test.c editor.h ../filer/core.c ../filer/host.c ../filer/sjis.h | $(BUILD)
	$(CC) -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-pie -no-pie -DFD_LIBRARY editor.c host_test.c ../filer/core.c ../filer/host.c -o $@
test: $(BUILD)/editor-host
	python3 -m unittest discover -s . -p 'test_*.py'
.PHONY: all test
