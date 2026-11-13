BUILD ?= build
all: $(BUILD)/K32CPU.COM $(BUILD)/K32MEM.EXE $(BUILD)/K32RUN.EXE apps
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
test: all $(BUILD)/test-resources4
	$(BUILD)/test-resources4
	python3 test_probe.py
	python3 test_runtime.py
	python3 test_elf.py
	python3 test_services.py
	$(MAKE) $(BUILD)/test-workspace
	$(BUILD)/test-workspace
$(BUILD)/test-workspace: test-workspace.c workspace.c workspace.h services.c services.h ../filer/sjis.h | $(BUILD)
	$(CC) -Wall -Wextra -Werror -fsanitize=address,undefined test-workspace.c workspace.c services.c -o $@
.PHONY: test
CFLAGS = -march=i386 -Os -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -msoft-float -mno-sse -mno-mmx -Wall -Wextra -Werror
$(BUILD)/run.o: runtime.S | $(BUILD)
	as --32 --defsym LOADER=1 $< -o $@
$(BUILD)/loader.o: loader.c elf.h services.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -c $< -o $@
$(BUILD)/services.o: services.c services.h elf.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -c $< -o $@
$(BUILD)/services-dos.o: services-dos.c services.h elf.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -c $< -o $@
$(BUILD)/elf.o: elf.c elf.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -c $< -o $@
$(BUILD)/libmemory.o: ../window/memory.c | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -c $< -o $@
$(BUILD)/verified-disk.o: ../filer/verified-disk.c ../filer/verified-disk.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -ffunction-sections -fdata-sections -c $< -o $@
$(BUILD)/run.elf: $(BUILD)/verified-disk.o $(BUILD)/run.o $(BUILD)/loader.o $(BUILD)/elf.o $(BUILD)/services.o $(BUILD)/services-dos.o $(BUILD)/libmemory.o runtime.ld
	ld -m elf_i386 -T runtime.ld $(filter %.o,$^) -o $@
$(BUILD)/K32RUN.EXE: $(BUILD)/run.elf ../mz/build.py
	python3 ../mz/build.py $< $@
$(BUILD)/crt0.o: sdk/crt0.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/wide.o: apps/wide.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/%.app.o: apps/%.c sdk/k32.h | $(BUILD)
	$(CC) -m32 $(CFLAGS) -Isdk -c $< -o $@
$(BUILD)/%.app.elf: $(BUILD)/%.app.o $(BUILD)/crt0.o sdk/app.ld
	ld -m elf_i386 -T sdk/app.ld $(filter %.o,$^) -o $@
$(BUILD)/memory.app.elf: $(BUILD)/memory.app.o $(BUILD)/crt0.o $(BUILD)/wide.o sdk/app.ld
	ld -m elf_i386 -T sdk/app.ld $(filter %.o,$^) -o $@
$(BUILD)/HELLO32.EXE: $(BUILD)/hello.app.elf package.py
	python3 package.py $< $@
$(BUILD)/MEM32.EXE: $(BUILD)/memory.app.elf package.py
	python3 package.py $< $@
$(BUILD)/SPIN32.EXE: $(BUILD)/spin.app.elf package.py
	python3 package.py $< $@
$(BUILD)/BADPTR32.EXE: $(BUILD)/badptr.app.elf package.py
	python3 package.py $< $@
$(BUILD)/FAULT32.EXE: $(BUILD)/fault.app.elf package.py
	python3 package.py $< $@
apps: $(BUILD)/HELLO32.EXE $(BUILD)/MEM32.EXE $(BUILD)/SPIN32.EXE $(BUILD)/BADPTR32.EXE $(BUILD)/FAULT32.EXE
.PHONY: apps
$(BUILD)/invalid.stamp: $(BUILD)/HELLO32.EXE make-invalid.py
	python3 make-invalid.py $(BUILD)
	touch $@
apps: $(BUILD)/invalid.stamp

$(BUILD)/crt1.o: sdk/crt1.S | $(BUILD)
	as --32 $< -o $@
$(BUILD)/%.ext.o: apps/%.c sdk/k32.h sdk/k32x.h | $(BUILD)
	$(CC) -m32 $(CFLAGS) -Isdk -c $< -o $@
$(BUILD)/%.ext.elf: $(BUILD)/%.ext.o $(BUILD)/crt1.o sdk/app.ld
	ld -m elf_i386 -T sdk/app.ld $(filter %.o,$^) -o $@
$(BUILD)/ARGS32.EXE: $(BUILD)/arguments.ext.elf package.py
	python3 package.py $< $@ --abi 257 --features 15
apps: $(BUILD)/ARGS32.EXE
$(BUILD)/HEAP32.EXE: $(BUILD)/heap.ext.elf package.py
	python3 package.py $< $@ --abi 257 --features 15
apps: $(BUILD)/HEAP32.EXE
$(BUILD)/INPUT32.EXE: $(BUILD)/input.ext.elf package.py
	python3 package.py $< $@ --abi 257 --features 15
apps: $(BUILD)/INPUT32.EXE
$(BUILD)/FILE32.EXE: $(BUILD)/file.ext.elf package.py
	python3 package.py $< $@ --abi 257 --features 15
apps: $(BUILD)/FILE32.EXE
$(BUILD)/VIEW32.EXE: $(BUILD)/view.ext.elf package.py
	python3 package.py $< $@ --abi 257 --features 15
apps: $(BUILD)/VIEW32.EXE

$(BUILD)/host-run.o: runtime.S host-entry.S host-engine.S | $(BUILD)
	as --32 --defsym LOADER=1 --defsym HOST=1 $< -o $@
$(BUILD)/%.host.o: %.c services.h elf.h host.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -DK32_HOST -c $< -o $@
$(BUILD)/host.elf: $(BUILD)/verified-disk.o $(BUILD)/workspace.host.o $(BUILD)/manager.host.o $(BUILD)/host-resources.o $(BUILD)/host-report.o $(BUILD)/host-run.o $(BUILD)/host.host.o $(BUILD)/loader.host.o $(BUILD)/elf.host.o $(BUILD)/services.host.o $(BUILD)/services-dos.host.o $(BUILD)/libmemory.o host.ld
	ld -m elf_i386 -T host.ld $(filter %.o,$^) -o $@
$(BUILD)/K32HOST.EXE: $(BUILD)/host.elf ../mz/build.py Makefile
	python3 ../mz/build.py --desktop-app "PM32 resident service" $< $@
all: $(BUILD)/K32HOST.EXE
$(BUILD)/mix.o: mix.c host.h ../window/k386_client.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/mix-client.o: ../window/k386_client.c | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/mix-dos.o: ../filer/dos.c | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -DDOS -DPC98 -c $< -o $@
$(BUILD)/mix-start.o: ../filer/start.s | $(BUILD)
	as --32 $< -o $@
$(BUILD)/mix-core.o: ../filer/core.c | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -DDOS -c $< -o $@
$(BUILD)/mix.elf: $(BUILD)/mix.o $(BUILD)/mix-client.o $(BUILD)/mix-dos.o $(BUILD)/mix-start.o $(BUILD)/mix-core.o $(BUILD)/libmemory.o ../filer/com.ld
	ld -m elf_i386 --gc-sections -T ../filer/com.ld $(filter %.o,$^) -o $@
$(BUILD)/K32MIX.COM: $(BUILD)/mix.elf
	objcopy -O binary $< $@
all: $(BUILD)/K32MIX.COM

$(BUILD)/host-resources.o: ../k386/resources.c ../k386/resources.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -DK32_HOST -c $< -o $@
$(BUILD)/host-report.o: ../k386/report.c ../k386/resources.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -DK32_HOST -c $< -o $@
$(BUILD)/host.host.o $(BUILD)/manager.host.o: manager.h
$(BUILD)/COUNT32.EXE: $(BUILD)/count.ext.elf package.py
	python3 package.py $< $@ --abi 257 --features 15
apps: $(BUILD)/COUNT32.EXE

$(BUILD)/XARRAY32.EXE: $(BUILD)/xarray.ext.elf package.py
	python3 package.py $< $@ --abi 257 --features 7
apps: $(BUILD)/XARRAY32.EXE

$(BUILD)/host.host.o $(BUILD)/services-dos.host.o $(BUILD)/services-dos.o: ../kgui32/sdk/protocol.h
$(BUILD)/workspace.host.o $(BUILD)/services-dos.host.o: workspace.h

$(BUILD)/bank.host.o: ../k386/bank.c ../k386/bank.h | $(BUILD)
	$(CC) -m16 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 $(CFLAGS) -DK32_HOST -c $< -o $@
$(BUILD)/host.elf: $(BUILD)/bank.host.o $(BUILD)/host-bank.host.o

$(BUILD)/test-resources4: resources4-test.c ../k386/resources.c ../k386/resources.h | $(BUILD)
	$(CC) -Wall -Wextra -Werror -DK32_HOST resources4-test.c ../k386/resources.c -o $@
