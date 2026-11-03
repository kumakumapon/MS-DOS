BUILD ?= build
all: $(BUILD)/K32CPU.COM $(BUILD)/K32MEM.EXE $(BUILD)/K32RUN.EXE apps
$(BUILD):
	mkdir -p $@
$(BUILD)/probe.o: probe.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/probe.elf: $(BUILD)/probe.o probe.ld
	ld -m elf_i386 -T probe.ld $< -o $@
$(BUILD)/K32CPU.COM: $(BUILD)/probe.elf
	objcopy -O binary $< $@
.PHONY: all
$(BUILD)/runtime.o: runtime.S memory-app.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/runtime.elf: $(BUILD)/runtime.o runtime.ld
	ld -m elf_i386 -T runtime.ld $< -o $@
$(BUILD)/K32MEM.EXE: $(BUILD)/runtime.elf ../mz/build.py
	python3 ../mz/build.py $< $@
test: all
	python3 test_probe.py
	python3 test_runtime.py
	python3 test_elf.py
.PHONY: test
CFLAGS = -march=i386 -Os -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -msoft-float -mno-sse -mno-mmx -Wall -Wextra -Werror
$(BUILD)/run.o: runtime.S | $(BUILD)
	as --32 --defsym LOADER=1 $< -o $@
$(BUILD)/loader.o: loader.c elf.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -c $< -o $@
$(BUILD)/elf.o: elf.c elf.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -c $< -o $@
$(BUILD)/libmemory.o: ../window/memory.c | $(BUILD)
	$(CC) -m16 $(CFLAGS) -c $< -o $@
$(BUILD)/run.elf: $(BUILD)/run.o $(BUILD)/loader.o $(BUILD)/elf.o $(BUILD)/libmemory.o runtime.ld
	ld -m elf_i386 -T runtime.ld $(filter %.o,$^) -o $@
$(BUILD)/K32RUN.EXE: $(BUILD)/run.elf ../mz/build.py
	python3 ../mz/build.py $< $@
$(BUILD)/crt0.o: sdk/crt0.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/wide.o: apps/wide.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/%.app.o: apps/%.c sdk/k32.h | $(BUILD)
	$(CC) -m32 $(CFLAGS) -Isdk -c $< -o $@
$(BUILD)/%.app.elf: $(BUILD)/%.app.o $(BUILD)/crt0.o sdk/app.ld
	ld -m elf_i386 -T sdk/app.ld $(filter %.o,$^) -o $@
$(BUILD)/memory.app.elf: $(BUILD)/memory.app.o $(BUILD)/crt0.o $(BUILD)/wide.o sdk/app.ld
	ld -m elf_i386 -T sdk/app.ld $(filter %.o,$^) -o $@
$(BUILD)/HELLO32.EXE: $(BUILD)/hello.app.elf package.py
	python3 package.py $< $@
$(BUILD)/MEM32.EXE: $(BUILD)/memory.app.elf package.py
	python3 package.py $< $@
$(BUILD)/SPIN32.EXE: $(BUILD)/spin.app.elf package.py
	python3 package.py $< $@
$(BUILD)/BADPTR32.EXE: $(BUILD)/badptr.app.elf package.py
	python3 package.py $< $@
$(BUILD)/FAULT32.EXE: $(BUILD)/fault.app.elf package.py
	python3 package.py $< $@
apps: $(BUILD)/HELLO32.EXE $(BUILD)/MEM32.EXE $(BUILD)/SPIN32.EXE $(BUILD)/BADPTR32.EXE $(BUILD)/FAULT32.EXE
.PHONY: apps
$(BUILD)/invalid.stamp: $(BUILD)/HELLO32.EXE make-invalid.py
	python3 make-invalid.py $(BUILD)
	touch $@
apps: $(BUILD)/invalid.stamp
