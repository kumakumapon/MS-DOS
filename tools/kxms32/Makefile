BUILD ?= build
CFLAGS = -m16 -march=i386 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 -Os -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -Wall -Wextra -Werror
all: $(BUILD)/KXMS32.SYS
$(BUILD):
	mkdir -p $@
$(BUILD)/entry.o: entry.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/%.o: %.c policy.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/memory.o: ../window/memory.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/driver.elf: $(BUILD)/entry.o $(BUILD)/driver.o $(BUILD)/policy.o $(BUILD)/memory.o driver.ld
	ld -m elf_i386 -T driver.ld $(filter %.o,$^) -o $@
$(BUILD)/KXMS32.SYS: $(BUILD)/driver.elf
	objcopy -O binary $< $@
$(BUILD)/test-policy: test-policy.c policy.c policy.h | $(BUILD)
	$(CC) -Wall -Wextra -Werror -fsanitize=address,undefined test-policy.c policy.c -o $@
test: all $(BUILD)/test-policy
	$(BUILD)/test-policy
.PHONY: all test

all: $(BUILD)/KXHIGH.SYS
$(BUILD)/capture.o: capture.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/capture.elf: $(BUILD)/capture.o capture.ld
	ld -m elf_i386 -T capture.ld $< -o $@
$(BUILD)/KXHIGH.SYS: $(BUILD)/capture.elf
	objcopy -O binary $< $@
