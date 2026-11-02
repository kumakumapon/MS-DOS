BUILD ?= build
all: $(BUILD)/K32CPU.COM
$(BUILD):
	mkdir -p $@
$(BUILD)/probe.o: probe.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/probe.elf: $(BUILD)/probe.o probe.ld
	ld -m elf_i386 -T probe.ld $< -o $@
$(BUILD)/K32CPU.COM: $(BUILD)/probe.elf
	objcopy -O binary $< $@
.PHONY: all
test: all
	python3 test_probe.py
.PHONY: test
