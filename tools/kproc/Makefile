CC ?= gcc
BUILD ?= build
CFLAGS = -m16 -march=i386 -Oz -mregparm=3 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -msoft-float -mno-sse -mno-mmx -ffunction-sections -fdata-sections -Wall -Wextra -Werror
all: $(BUILD)/K32PROC.EXE
$(BUILD):
	mkdir -p $@
$(BUILD)/%.o: %.c process.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/entry.o: entry.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/memory.o: ../window/memory.c | $(BUILD)
	$(CC) $(CFLAGS) -DK32_FAST_RAM -c $< -o $@
$(BUILD)/driver.elf: $(BUILD)/entry.o $(BUILD)/driver.o $(BUILD)/process.o $(BUILD)/memory.o driver.ld
	ld -m elf_i386 --gc-sections -T driver.ld $(filter %.o,$^) -o $@
$(BUILD)/K32PROC.EXE: $(BUILD)/driver.elf ../mz/build.py
	python3 ../mz/build.py --desktop-app "Process and stream broker" $< $@
$(BUILD)/test-process: test-process.c process.c process.h | $(BUILD)
	$(CC) -Wall -Wextra -Werror -fsanitize=address,undefined -g test-process.c process.c -o $@
$(BUILD)/test-shell: test-shell.c shell.c shell.h | $(BUILD)
	$(CC) -Wall -Wextra -Werror -fsanitize=address,undefined test-shell.c shell.c -o $@
test: $(BUILD)/test-process $(BUILD)/test-shell
	$(BUILD)/test-process
	$(BUILD)/test-shell
.PHONY: all test
