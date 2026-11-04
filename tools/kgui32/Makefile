CC ?= gcc
BUILD ?= build
K32 = ../k38632
CFLAGS = -march=i386 -Os -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -msoft-float -mno-sse -mno-mmx -ffunction-sections -fdata-sections -Wall -Wextra -Werror
NATIVE = $(CFLAGS) -m16 -DDOS -DPC98 -DK32_HOST -DK32_GUI
all: $(BUILD)/KGUILDR.EXE $(BUILD)/KGUI32.EXE
all: $(BUILD)/KGPROOF.COM
$(BUILD):
	mkdir -p $@
$(BUILD)/runtime.o: $(K32)/runtime.S $(K32)/host-engine.S entry.S | $(BUILD)
	as --32 -I $(K32) --defsym LOADER=1 --defsym HOST=1 --defsym GUI_HOST=1 $< -o $@
$(BUILD)/driver.o: driver.c gui.h | $(BUILD)
	$(CC) $(NATIVE) -c $< -o $@
$(BUILD)/%.broker.o: $(K32)/%.c $(K32)/elf.h $(K32)/services.h | $(BUILD)
	$(CC) $(NATIVE) -c $< -o $@
$(BUILD)/hardware.o: ../window/hardware.c ../window/hardware.h | $(BUILD)
	$(CC) $(NATIVE) -DKGUI32_DRIVER -c $< -o $@
$(BUILD)/memory.o: ../window/memory.c | $(BUILD)
	$(CC) $(NATIVE) -c $< -o $@
$(BUILD)/cirrus.o: ../display/cirrus.c | $(BUILD)
	$(CC) $(NATIVE) -c $< -o $@
$(BUILD)/dos.o: ../filer/dos.c | $(BUILD)
	$(CC) $(NATIVE) -c $< -o $@
$(BUILD)/core.o: ../filer/core.c | $(BUILD)
	$(CC) $(NATIVE) -c $< -o $@
$(BUILD)/client.o: ../window/k386_client.c | $(BUILD)
	$(CC) $(NATIVE) -c $< -o $@
$(BUILD)/driver.elf: $(BUILD)/runtime.o $(BUILD)/driver.o $(BUILD)/loader.broker.o $(BUILD)/elf.broker.o $(BUILD)/services.broker.o $(BUILD)/services-dos.broker.o $(BUILD)/hardware.o $(BUILD)/memory.o $(BUILD)/cirrus.o $(BUILD)/dos.o $(BUILD)/core.o $(BUILD)/client.o driver.ld
	ld -m elf_i386 --gc-sections -T driver.ld $(filter %.o,$^) -o $@
$(BUILD)/KGUILDR.EXE: $(BUILD)/driver.elf ../mz/build.py
	python3 ../mz/build.py --desktop-app 'KGUI32 native loader' $< $@
$(BUILD)/desktop.o: desktop.c gui.h | $(BUILD)
	$(CC) $(CFLAGS) -m32 -I$(K32)/sdk -c $< -o $@
$(BUILD)/scene.o: scene.c scene.h gui.h | $(BUILD)
	$(CC) $(CFLAGS) -m32 -c $< -o $@
$(BUILD)/crt.o: $(K32)/sdk/crt1.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/desktop.elf: $(BUILD)/desktop.o $(BUILD)/scene.o $(BUILD)/crt.o $(K32)/sdk/app.ld
	ld -m elf_i386 -T $(K32)/sdk/app.ld $(filter %.o,$^) -o $@
$(BUILD)/KGUI32.EXE: $(BUILD)/desktop.elf $(K32)/package.py
	python3 $(K32)/package.py $< $@ --abi 257 --features 23 --gui
$(BUILD)/proof.o: proof.c ../k38632/host.h ../window/k386_client.h | $(BUILD)
	$(CC) $(NATIVE) -c $< -o $@
$(BUILD)/proof-start.o: ../filer/start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/proof.elf: $(BUILD)/proof.o $(BUILD)/proof-start.o $(BUILD)/hardware.o $(BUILD)/memory.o $(BUILD)/dos.o $(BUILD)/core.o $(BUILD)/client.o ../filer/com.ld
	ld -m elf_i386 --gc-sections -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/KGPROOF.COM: $(BUILD)/proof.elf
	objcopy -O binary $< $@
$(BUILD)/test-scene: test-scene.c scene.c scene.h gui.h | $(BUILD)
	$(CC) -Wall -Wextra -Werror test-scene.c scene.c -o $@
test: all $(BUILD)/test-scene
	$(BUILD)/test-scene
	python3 test_image.py
.PHONY: all test
