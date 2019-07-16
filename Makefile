CC = gcc
AS = as
LD = ld


default: build kern
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j --no-print-directory


build:
	@mkdir -p build



CFLAGS=-fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -nostdinc -ffreestanding -fno-builtin $(CWARNINGS) $(CINCLUDES)

LDFLAGS := -z max-page-size=0x1000

kern:
	@nasm -f elf64 -w-zext-reloc -o build/kernel.o src/kernel/boot.asm

	@$(CC) -c -o build/ckern.o $(CFLAGS) src/kernel/boot.c
	@ld -T src/kernel/kernel.ld build/kernel.o build/ckern.o -o build/kernel.elf


clean:
	rm -rf build
