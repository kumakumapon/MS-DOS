K32 ?= ../..
CC ?= gcc
FLAGS = -m32 -march=i386 -g -Os -ffreestanding -fno-pie -fno-pic -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -msoft-float -mno-sse -mno-mmx -Wall -Wextra -Werror
all: APP.EXE
main.o: main.c
	$(CC) $(FLAGS) -I$(K32)/sdk -c $< -o $@
libc.o: $(K32)/sdk/libc.c
	$(CC) $(FLAGS) -c $< -o $@
crt.o: $(K32)/sdk/crt1.S
	as --32 $< -o $@
APP.ELF: main.o libc.o crt.o
	ld -m elf_i386 -T $(K32)/sdk/app.ld $^ -o $@
APP.EXE: APP.ELF
	python3 $(K32)/package.py $< $@ --abi 258 --features 15 --compact --stack-bytes 65536 --min-heap 65536 --debug-map APP.debug.json
.PHONY: all
