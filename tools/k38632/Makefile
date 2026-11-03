BUILD ?= build
all: $(BUILD)/K32CPU.COM $(BUILD)/K32MEM.EXE
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
.PHONY: test
