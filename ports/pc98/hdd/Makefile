BUILD ?= build
CFLAGS = -m16 -march=i386 -Os -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -Wall -Wextra -Werror
all: $(BUILD)/KHD98.SYS
all: $(BUILD)/HDDTEST.EXE
$(BUILD):
	mkdir -p $@
$(BUILD)/entry.o: driver.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/%.o: %.c policy.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/driver.elf: $(BUILD)/entry.o $(BUILD)/driver.o $(BUILD)/policy.o driver.ld
	ld -m elf_i386 -T driver.ld $(filter %.o,$^) -o $@
$(BUILD)/KHD98.SYS: $(BUILD)/driver.elf
	objcopy -O binary $< $@
$(BUILD)/probe-start.o: ../../../tools/mz/start.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/fault.o: fault.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/probe.elf: $(BUILD)/probe.o $(BUILD)/probe-start.o $(BUILD)/fault.o ../../../tools/mz/native.ld
	ld -m elf_i386 --no-check-sections -T ../../../tools/mz/native.ld $(filter %.o,$^) -o $@
$(BUILD)/HDDTEST.EXE: $(BUILD)/probe.elf ../../../tools/mz/build.py
	python3 ../../../tools/mz/build.py $< $@
.PHONY: all test
test: $(BUILD)/policy-test
	$(BUILD)/policy-test
$(BUILD)/policy-test: policy-test.c policy.c policy.h | $(BUILD)
	$(CC) -std=c99 -Wall -Wextra -Werror -fsanitize=undefined,address -g policy-test.c policy.c -o $@
