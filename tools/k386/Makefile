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
all: $(BUILD)/K386.EXE
$(BUILD)/monitor.o: monitor.c abi.h runtime.h resources.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/engine.o: engine.c abi.h runtime.h backing.h arena.h | $(BUILD)
	$(CC) -m32 $(CFLAGS) -c $< -o $@
$(BUILD)/transition.o: monitor.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/memory.o: ../window/memory.c | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/dos.o: ../filer/dos.c | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -DPC98 -c $< -o $@
$(BUILD)/runtime.o: runtime.c abi.h runtime.h loader.h arena.h backing.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/loader.o: loader.c loader.h abi.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -c $< -o $@
$(BUILD)/resources.o: resources.c resources.h abi.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -c $< -o $@
$(BUILD)/report.o: report.c resources.h abi.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -c $< -o $@
$(BUILD)/monitor.elf: $(BUILD)/transition.o $(BUILD)/monitor.o $(BUILD)/runtime.o $(BUILD)/loader.o $(BUILD)/backing.o $(BUILD)/arena.o $(BUILD)/engine.o $(BUILD)/memory.o $(BUILD)/dos.o $(BUILD)/core.o $(BUILD)/resources.o $(BUILD)/report.o monitor.ld
	ld -m elf_i386 --gc-sections -T monitor.ld $(filter %.o,$^) -o $@
$(BUILD)/K386.EXE: $(BUILD)/monitor.elf ../mz/build.py
	python3 ../mz/build.py $< $@
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

$(BUILD)/loader-test: loader_test.c loader.c loader.h abi.h | $(BUILD)
	$(CC) -O2 -Wall -Wextra -Werror loader_test.c loader.c -o $@
$(BUILD)/resources-test: resources_test.c resources.c resources.h abi.h | $(BUILD)
	$(CC) -O2 -Wall -Wextra -Werror resources_test.c resources.c -o $@
test: $(BUILD)/loader-test $(BUILD)/arena-test $(BUILD)/resources-test
	$(BUILD)/loader-test
	$(BUILD)/arena-test
	$(BUILD)/resources-test
.PHONY: test

all: $(BUILD)/INTWAIT.COM $(BUILD)/KRESCHK.COM
$(BUILD)/INTWAIT.COM: $(BUILD)/interrupt.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/interrupt.elf
	objcopy -O binary $(BUILD)/interrupt.elf $@
$(BUILD)/resourcecheck.o: resourcecheck.c resources.h abi.h ../window/k386_client.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/resource-client.o: ../window/k386_client.c ../window/k386_client.h resources.h abi.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/resourcecheck.elf: $(BUILD)/resourcecheck.o $(BUILD)/resource-client.o $(BUILD)/start.o $(BUILD)/memory.o $(BUILD)/dos.o $(BUILD)/core.o ../filer/com.ld
	ld -m elf_i386 --gc-sections -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/KRESCHK.COM: $(BUILD)/resourcecheck.elf
	objcopy -O binary $< $@
all: $(BUILD)/KSH.COM
$(BUILD)/shell.o: shell.c ../filer/filer.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/shell.elf: $(BUILD)/shell.o $(BUILD)/start.o $(BUILD)/dos.o $(BUILD)/core.o ../filer/com.ld
	ld -m elf_i386 --gc-sections -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/KSH.COM: $(BUILD)/shell.elf
	objcopy -O binary $(BUILD)/shell.elf $@
all: $(BUILD)/MZTEST.EXE $(BUILD)/FLAGS.COM
$(BUILD)/MZTEST.EXE: $(BUILD)/mz.sample.o mz.ld
	ld -m elf_i386 -T mz.ld $< -o $(BUILD)/mz.elf
	objcopy -O binary $(BUILD)/mz.elf $@
$(BUILD)/FLAGS.COM: $(BUILD)/flags.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/flags.elf
	objcopy -O binary $(BUILD)/flags.elf $@
all: $(BUILD)/DTA.COM
$(BUILD)/DTA.COM: $(BUILD)/dta.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/dta.elf
	objcopy -O binary $(BUILD)/dta.elf $@
all: $(BUILD)/KEYCHK.COM
$(BUILD)/KEYCHK.COM: $(BUILD)/keycheck.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/keycheck.elf
	objcopy -O binary $(BUILD)/keycheck.elf $@

$(BUILD)/backing.o: backing.c backing.h abi.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -c $< -o $@
$(BUILD)/arena.o: arena.c arena.h abi.h | $(BUILD)
	$(CC) -m16 $(CFLAGS) -c $< -o $@
$(BUILD)/arena-test: arena.c arena_test.c arena.h abi.h | $(BUILD)
	$(CC) -O2 -Wall -Wextra -Werror arena.c arena_test.c -o $@

all: $(BUILD)/TIMER.COM $(BUILD)/MEMORY.COM
$(BUILD)/TIMER.COM: $(BUILD)/timer.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/timer.elf
	objcopy -O binary $(BUILD)/timer.elf $@
$(BUILD)/MEMORY.COM: $(BUILD)/memorycheck.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/memorycheck.elf
	objcopy -O binary $(BUILD)/memorycheck.elf $@
all: $(BUILD)/PROCESS.COM
$(BUILD)/PROCESS.COM: $(BUILD)/processcheck.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/processcheck.elf
	objcopy -O binary $(BUILD)/processcheck.elf $@
all: $(BUILD)/BADTIMER.COM
$(BUILD)/BADTIMER.COM: $(BUILD)/badtimer.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/badtimer.elf
	objcopy -O binary $(BUILD)/badtimer.elf $@
all: $(BUILD)/ROMWRITE.COM
$(BUILD)/ROMWRITE.COM: $(BUILD)/romwrite.sample.o com.ld
	ld -m elf_i386 -T com.ld $< -o $(BUILD)/romwrite.elf
	objcopy -O binary $(BUILD)/romwrite.elf $@
