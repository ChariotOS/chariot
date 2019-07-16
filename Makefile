CC = gcc
AS = as
LD = ld


default:
	@mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j --no-print-directory

kern:
	nasm -o kernel.bin kernel/boot.asm
	@#$(AS) --32 -o boot.o kernel/boot.asm
	@#$(LD) $(LDFLAGS) -m elf_i386 --oformat binary -o kernel.bin boot.o

clean:
	rm -rf build
