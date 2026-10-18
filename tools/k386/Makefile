BUILD ?= build
all: $(BUILD)/K386CPU.COM
$(BUILD):
	mkdir -p $@
$(BUILD)/probe.o: probe.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/probe.elf: $(BUILD)/probe.o probe.ld
	ld -m elf_i386 -T probe.ld $< -o $@
$(BUILD)/K386CPU.COM: $(BUILD)/probe.elf
	objcopy -O binary $< $@
.PHONY: all
CFLAGS = -march=i386 -Os -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -ffunction-sections -fdata-sections -Wall -Wextra -Werror
all: $(BUILD)/K386.COM
$(BUILD)/monitor.o: monitor.c abi.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/engine.o: engine.c abi.h | $(BUILD)
	$(CC) -m32 $(CFLAGS) -c $< -o $@
$(BUILD)/transition.o: monitor.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/memory.o: ../window/memory.c | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/dos.o: ../filer/dos.c | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -DPC98 -c $< -o $@
$(BUILD)/monitor.elf: $(BUILD)/transition.o $(BUILD)/monitor.o $(BUILD)/engine.o $(BUILD)/memory.o $(BUILD)/dos.o monitor.ld
	ld -m elf_i386 --gc-sections -T monitor.ld $(filter %.o,$^) -o $@
$(BUILD)/K386.COM: $(BUILD)/monitor.elf
	objcopy -O binary $< $@
all: $(BUILD)/COUNTER.COM $(BUILD)/ECHO.COM $(BUILD)/FAULT.COM
$(BUILD)/%.sample.o: %.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/COUNTER.COM: $(BUILD)/counter.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/counter.elf
	objcopy -O binary $(BUILD)/counter.elf $@
$(BUILD)/ECHO.COM: $(BUILD)/echo.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/echo.elf
	objcopy -O binary $(BUILD)/echo.elf $@
$(BUILD)/FAULT.COM: $(BUILD)/fault.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/fault.elf
	objcopy -O binary $(BUILD)/fault.elf $@
all: $(BUILD)/K386TEST.COM
$(BUILD)/check.o: check.c abi.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/start.o: ../filer/start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/check.elf: $(BUILD)/check.o $(BUILD)/start.o $(BUILD)/memory.o $(BUILD)/dos.o $(BUILD)/core.o ../filer/com.ld
	ld -m elf_i386 --gc-sections -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/K386TEST.COM: $(BUILD)/check.elf
	objcopy -O binary $< $@

$(BUILD)/core.o: ../filer/core.c | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
all: $(BUILD)/KFILE.COM
$(BUILD)/KFILE.COM: $(BUILD)/filecheck.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/filecheck.elf
	objcopy -O binary $(BUILD)/filecheck.elf $@
all: $(BUILD)/PORT.COM
$(BUILD)/PORT.COM: $(BUILD)/port.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/port.elf
	objcopy -O binary $(BUILD)/port.elf $@
all: $(BUILD)/DF.COM
$(BUILD)/DF.COM: $(BUILD)/direction.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/direction.elf
	objcopy -O binary $(BUILD)/direction.elf $@
all: $(BUILD)/FPU.COM
$(BUILD)/FPU.COM: $(BUILD)/fpu.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/fpu.elf
	objcopy -O binary $(BUILD)/fpu.elf $@
