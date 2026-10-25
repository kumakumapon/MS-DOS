CC ?= gcc
BUILD ?= build
CFLAGS = -m16 -march=i386 -Os -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -Wall -Wextra -Werror -DDOS

all: $(BUILD)/FD.COM $(BUILD)/FD98.EXE $(BUILD)/FILER.TXT $(BUILD)/FILERLIC.TXT
$(BUILD):
	mkdir -p $@
$(BUILD)/core.o: core.c filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/ui.o: ui.c filer.h sjis.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/dos.o: dos.c filer.h sjis.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/dos98.o: dos.c filer.h sjis.h | $(BUILD)
	$(CC) $(CFLAGS) -DPC98 -DDOS_MZ -c $< -o $@
$(BUILD)/start.o: start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/fd.elf: $(BUILD)/start.o $(BUILD)/core.o $(BUILD)/ui.o $(BUILD)/dos.o com.ld
	ld -m elf_i386 -T com.ld $(filter %.o,$^) -o $@
$(BUILD)/exe-start.o: ../mz/start.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/fd98.elf: $(BUILD)/exe-start.o $(BUILD)/core.o $(BUILD)/ui.o $(BUILD)/dos98.o ../mz/native.ld
	ld -m elf_i386 --no-check-sections -T ../mz/native.ld $(filter %.o,$^) -o $@
$(BUILD)/FD.COM: $(BUILD)/fd.elf
	objcopy -O binary $< $@
$(BUILD)/FD98.EXE: $(BUILD)/fd98.elf ../mz/build.py
	python3 ../mz/build.py --desktop-app 'File manager' $< $@
$(BUILD)/FILER.TXT: FILER.TXT | $(BUILD)
	python3 -c "from pathlib import Path; Path('$@').write_bytes(Path('$<').read_text().replace('\n', '\r\n').encode('ascii'))"
$(BUILD)/FILERLIC.TXT: LICENSE | $(BUILD)
	cp $< $@
$(BUILD)/filer-host: core.c host.c filer.h | $(BUILD)
	$(CC) -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-pie -no-pie core.c host.c -o $@
host: $(BUILD)/filer-host
test: all host
	python3 -m unittest discover -s . -p 'test_*.py'
.PHONY: all host test

$(BUILD)/core.o $(BUILD)/ui.o $(BUILD)/dos.o $(BUILD)/dos98.o: Makefile
