CC = gcc
AS = as
LD = ld


default:
	@mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j --no-print-directory
	@nasm -f elf -o build/kernel.elf src/kernel/boot.asm

clean:
	rm -rf build
