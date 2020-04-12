.PHONY: default

default:
	@nasm -f elf64 -o rsrt0.o rsrt0.asm
	xargo build --target chariot-x86_64
