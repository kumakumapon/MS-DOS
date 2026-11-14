CC ?= gcc
BUILD ?= build
CFLAGS = -m16 -march=i386 -Oz -mregparm=3 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -msoft-float -mno-sse -mno-mmx -ffunction-sections -fdata-sections -Wall -Wextra -Werror
all: $(BUILD)/K32IO.EXE
$(BUILD):
	mkdir -p $@
$(BUILD)/driver.o: driver.c ../k38632/io.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/entry.o: entry.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/policy.o: ../k38632/io-policy.c ../k38632/io.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/services.o: ../k38632/services.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/memory.o: ../window/memory.c Makefile | $(BUILD)
	$(CC) $(CFLAGS) -DK32_FAST_RAM -c $< -o $@
$(BUILD)/disk.o: ../filer/verified-disk.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/driver.elf: $(BUILD)/entry.o $(BUILD)/driver.o $(BUILD)/policy.o $(BUILD)/services.o $(BUILD)/memory.o $(BUILD)/disk.o driver.ld
	ld -m elf_i386 --gc-sections -T driver.ld $(filter %.o,$^) -o $@
$(BUILD)/K32IO.EXE: $(BUILD)/driver.elf ../mz/build.py
	python3 ../mz/build.py --desktop-app "Work file broker" $< $@
$(BUILD)/test-stream: test-stream.c driver.c ../k38632/io-policy.c ../k38632/services.c | $(BUILD)
	$(CC) -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable -fsanitize=address,undefined test-stream.c ../k38632/io-policy.c ../k38632/services.c -o $@
test: all $(BUILD)/test-stream
	$(BUILD)/test-stream
.PHONY: all test
