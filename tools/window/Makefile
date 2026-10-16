CC ?= gcc
BUILD ?= build
CFLAGS = -m16 -march=i386 -Os -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -ffunction-sections -fdata-sections -Wall -Wextra -Werror -DDOS

all: $(BUILD)/KVIEW.COM $(BUILD)/KPAINT.COM $(BUILD)/COLORS.BMP $(BUILD)/KIMAGE.TXT $(BUILD)/KDESK.COM $(BUILD)/KDESK.TXT $(BUILD)/KDLIC.TXT
$(BUILD):
	mkdir -p $@
$(BUILD)/start.o: ../filer/start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/core.o: ../filer/core.c ../filer/filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/dos98.o: ../filer/dos.c ../filer/filer.h ../filer/sjis.h | $(BUILD)
	$(CC) $(CFLAGS) -DPC98 -c $< -o $@
$(BUILD)/window.o: window.c hardware.h clipboard.h utilities.h ../filer/filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/utilities.o: utilities.c utilities.h hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/clipboard.o: clipboard.c clipboard.h hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/memory.o: memory.c hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/hardware.o: hardware.c hardware.h ../filer/filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/window.elf: $(BUILD)/window.o $(BUILD)/utilities.o $(BUILD)/hardware.o $(BUILD)/memory.o $(BUILD)/clipboard.o $(BUILD)/start.o $(BUILD)/core.o $(BUILD)/dos98.o ../filer/com.ld
	ld -m elf_i386 --gc-sections -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/KDESK.COM: $(BUILD)/window.elf
	objcopy -O binary $< $@
$(BUILD)/KDESK.TXT: KDESK.TXT | $(BUILD)
	python3 -c "from pathlib import Path; Path('$@').write_bytes(Path('$<').read_text().replace('\n', '\r\n').encode('ascii'))"
$(BUILD)/KDLIC.TXT: LICENSE | $(BUILD)
	cp $< $@
.PHONY: all

$(BUILD)/bitmap.o: bitmap.c bitmap.h hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/paint.o: imageapp.c bitmap.h clipboard.h hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/view.o: imageapp.c bitmap.h clipboard.h hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -DVIEW_ONLY=1 -c $< -o $@
$(BUILD)/paint.elf: $(BUILD)/paint.o $(BUILD)/bitmap.o $(BUILD)/hardware.o $(BUILD)/memory.o $(BUILD)/clipboard.o $(BUILD)/start.o $(BUILD)/core.o $(BUILD)/dos98.o ../filer/com.ld
	ld -m elf_i386 --gc-sections -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/view.elf: $(BUILD)/view.o $(BUILD)/bitmap.o $(BUILD)/hardware.o $(BUILD)/memory.o $(BUILD)/clipboard.o $(BUILD)/start.o $(BUILD)/core.o $(BUILD)/dos98.o ../filer/com.ld
	ld -m elf_i386 --gc-sections -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/KPAINT.COM: $(BUILD)/paint.elf
	objcopy -O binary $< $@
$(BUILD)/KVIEW.COM: $(BUILD)/view.elf
	objcopy -O binary $< $@
$(BUILD)/COLORS.BMP: sample.py | $(BUILD)
	python3 $< $@
$(BUILD)/KIMAGE.TXT: KIMAGE.TXT | $(BUILD)
	python3 -c "from pathlib import Path; Path('$@').write_bytes(Path('$<').read_text().replace('\n', '\r\n').encode('ascii'))"

$(BUILD)/calculator-test: calculator_test.c utilities.c utilities.h ../filer/core.c | $(BUILD)
	$(CC) -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DCALCULATOR_TEST calculator_test.c utilities.c ../filer/core.c -Wl,--gc-sections -o $@
test: $(BUILD)/calculator-test
	$(BUILD)/calculator-test
.PHONY: test
