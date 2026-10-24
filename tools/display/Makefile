BUILD ?= build
CFLAGS = -m16 -march=i386 -Os -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -ffunction-sections -fdata-sections -DDOS -DPC98 -Wall -Wextra -Werror
all: $(BUILD)/KDISP.COM
$(BUILD):
	mkdir -p $@
$(BUILD)/cirrus.o: cirrus.c cirrus.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/probe.o: probe.c cirrus.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/memory.o: ../window/memory.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/dos.o: ../filer/dos.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/core.o: ../filer/core.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/start.o: ../filer/start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/probe.elf: $(BUILD)/probe.o $(BUILD)/cirrus.o $(BUILD)/memory.o $(BUILD)/dos.o $(BUILD)/core.o $(BUILD)/start.o ../filer/com.ld
	ld -m elf_i386 --gc-sections -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/KDISP.COM: $(BUILD)/probe.elf
	objcopy -O binary $< $@
