CC ?= gcc
BUILD ?= build
CFLAGS = -m16 -march=i386 -Os -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -msoft-float -mno-sse -mno-mmx -ffunction-sections -fdata-sections -Wall -Wextra -Werror
all: $(BUILD)/KV98VID.EXE
$(BUILD):
	mkdir -p $@
$(BUILD)/%.o: %.c video.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/memory.o: ../window/memory.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/entry.o: entry.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/driver.elf: $(BUILD)/entry.o $(BUILD)/driver.o $(BUILD)/video.o $(BUILD)/memory.o driver.ld
	ld -m elf_i386 --gc-sections -T driver.ld $(filter %.o,$^) -o $@
$(BUILD)/KV98VID.EXE: $(BUILD)/driver.elf ../mz/build.py
	python3 ../mz/build.py $< $@
$(BUILD)/test-video: test-video.c video.c video.h | $(BUILD)
	$(CC) -Wall -Wextra -Werror -fsanitize=address,undefined test-video.c video.c -o $@
test: all $(BUILD)/test-video
	$(BUILD)/test-video
.PHONY: all test

all: $(BUILD)/KVTERM.COM
$(BUILD)/terminal.o: terminal.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/KVTERM.COM: $(BUILD)/terminal.o ../k386/com.ld
	ld -m elf_i386 -T ../k386/com.ld $< -o $(BUILD)/terminal.elf
	objcopy -O binary $(BUILD)/terminal.elf $@
