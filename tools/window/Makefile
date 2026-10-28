CC ?= gcc
BUILD ?= build
# Native desktop and image tools use separate MZ code/data/stack segments.
CFLAGS = -m16 -march=i386 -mregparm=3 -mpreferred-stack-boundary=2 -Oz -flto -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -ffunction-sections -fdata-sections -Wall -Wextra -Werror -DDOS

all: $(BUILD)/KVIEW.EXE $(BUILD)/KPAINT.EXE $(BUILD)/COLORS.BMP $(BUILD)/KIMAGE.TXT $(BUILD)/KDESK.EXE $(BUILD)/KDESK.TXT $(BUILD)/KDLIC.TXT
$(BUILD):
	mkdir -p $@
$(BUILD)/start.o: ../filer/start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/exe-start.o: exe-start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/core.o: ../filer/core.c ../filer/filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/dos98.o: ../filer/dos.c ../filer/filer.h ../filer/sjis.h | $(BUILD)
	$(CC) $(CFLAGS) -DPC98 -c $< -o $@
$(BUILD)/exe-dos98.o: ../filer/dos.c ../filer/filer.h ../filer/sjis.h | $(BUILD)
	$(CC) $(CFLAGS) -DPC98 -DDOS_MZ -c $< -o $@
$(BUILD)/window.o: window.c commands.h k386_client.h ../k386/abi.h ../k386/resources.h tasks.h scroll.h hardware.h clipboard.h utilities.h ../filer/filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/k386_client.o: k386_client.c k386_client.h ../k386/abi.h ../k386/resources.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/tasks.o: tasks.c tasks.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/scroll.o: scroll.c scroll.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/utilities.o: utilities.c utilities.h hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/art.o: art.c art_data.h utilities.h hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/commands.o: commands.c commands.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/clipboard.o: clipboard.c clipboard.h hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/memory.o: memory.c hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/hardware.o: hardware.c hardware.h ../filer/filer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/windowtools.o: windowtools.c windowtools.h ../editor/editor.h | $(BUILD)
	$(CC) $(CFLAGS) -DED_CAP=8192 -c $< -o $@
$(BUILD)/windoweditor.o: ../editor/editor.c ../editor/editor.h | $(BUILD)
	$(CC) $(CFLAGS) -DED_CAP=8192 -c $< -o $@
$(BUILD)/windowcontrol.o: ../control/control.c | $(BUILD)
	$(CC) $(CFLAGS) -DKDESK_WINDOW -c $< -o $@
$(BUILD)/window.elf: $(BUILD)/windowtools.o $(BUILD)/windoweditor.o $(BUILD)/windowcontrol.o $(BUILD)/window.o $(BUILD)/cirrus.o $(BUILD)/app.o $(BUILD)/k386_client.o $(BUILD)/tasks.o $(BUILD)/scroll.o $(BUILD)/utilities.o $(BUILD)/art.o $(BUILD)/commands.o $(BUILD)/hardware.o $(BUILD)/memory.o $(BUILD)/clipboard.o $(BUILD)/exe-start.o $(BUILD)/core.o $(BUILD)/exe-dos98.o exe.ld
	$(CC) $(CFLAGS) -nostdlib -no-pie -Wl,--gc-sections,--no-check-sections,-T,exe.ld $(filter %.o,$^) -o $@
$(BUILD)/KDESK.EXE: $(BUILD)/window.elf ../mz/build.py
	python3 ../mz/build.py $< $@
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
$(BUILD)/paint.elf: $(BUILD)/paint.o $(BUILD)/bitmap.o $(BUILD)/cirrus.o $(BUILD)/hardware.o $(BUILD)/memory.o $(BUILD)/clipboard.o $(BUILD)/exe-start.o $(BUILD)/core.o $(BUILD)/exe-dos98.o exe.ld
	$(CC) $(CFLAGS) -nostdlib -no-pie -Wl,--gc-sections,--no-check-sections,-T,exe.ld $(filter %.o,$^) -o $@
$(BUILD)/view.elf: $(BUILD)/view.o $(BUILD)/bitmap.o $(BUILD)/cirrus.o $(BUILD)/hardware.o $(BUILD)/memory.o $(BUILD)/clipboard.o $(BUILD)/exe-start.o $(BUILD)/core.o $(BUILD)/exe-dos98.o exe.ld
	$(CC) $(CFLAGS) -nostdlib -no-pie -Wl,--gc-sections,--no-check-sections,-T,exe.ld $(filter %.o,$^) -o $@
$(BUILD)/KPAINT.EXE: $(BUILD)/paint.elf ../mz/build.py
	python3 ../mz/build.py --desktop-app 'Image editor' $< $@
$(BUILD)/KVIEW.EXE: $(BUILD)/view.elf ../mz/build.py
	python3 ../mz/build.py --desktop-app 'Image viewer' $< $@
$(BUILD)/COLORS.BMP: sample.py | $(BUILD)
	python3 $< $@
$(BUILD)/KIMAGE.TXT: KIMAGE.TXT | $(BUILD)
	python3 -c "from pathlib import Path; Path('$@').write_bytes(Path('$<').read_text().replace('\n', '\r\n').encode('ascii'))"

$(BUILD)/calculator-test: calculator_test.c utilities.c utilities.h ../filer/core.c | $(BUILD)
	$(CC) -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DCALCULATOR_TEST calculator_test.c utilities.c ../filer/core.c -Wl,--gc-sections -o $@
$(BUILD)/scroll-test: scroll_test.c scroll.c scroll.h | $(BUILD)
	$(CC) -O2 -Wall -Wextra -Werror scroll_test.c scroll.c -o $@
$(BUILD)/tasks-test: tasks_test.c tasks.c tasks.h | $(BUILD)
	$(CC) -O2 -Wall -Wextra -Werror tasks_test.c tasks.c -o $@
$(BUILD)/commands-test: commands_test.c commands.c commands.h | $(BUILD)
	$(CC) -O2 -Wall -Wextra -Werror commands_test.c commands.c -o $@
test: $(BUILD)/calculator-test $(BUILD)/scroll-test $(BUILD)/tasks-test $(BUILD)/commands-test
	$(BUILD)/calculator-test
	$(BUILD)/scroll-test
	$(BUILD)/tasks-test
	$(BUILD)/commands-test
	python3 -m unittest discover -s ../mz -p 'test_*.py'
.PHONY: test

# Rebuild every native object if the internal C calling convention changes.
$(BUILD)/window.o $(BUILD)/cirrus.o $(BUILD)/app.o $(BUILD)/k386_client.o $(BUILD)/tasks.o $(BUILD)/scroll.o $(BUILD)/utilities.o $(BUILD)/art.o $(BUILD)/commands.o $(BUILD)/hardware.o $(BUILD)/memory.o $(BUILD)/clipboard.o $(BUILD)/core.o $(BUILD)/dos98.o $(BUILD)/exe-dos98.o $(BUILD)/paint.o $(BUILD)/view.o $(BUILD)/bitmap.o: Makefile

$(BUILD)/app.o: ../k386/app.c ../k386/app.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/window.o: ../k386/app.h windowtools.h
$(BUILD)/windowtools.o $(BUILD)/windoweditor.o $(BUILD)/windowcontrol.o: Makefile

$(BUILD)/cirrus.o: ../display/cirrus.c ../display/cirrus.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
