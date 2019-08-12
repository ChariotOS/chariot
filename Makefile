CC = gcc
CXX = g++
AS = nasm
LD = ld

.PHONY: fs




STRUCTURE := $(shell find src -type d)
CODEFILES := $(addsuffix /*,$(STRUCTURE))
CODEFILES := $(wildcard $(CODEFILES))


CSOURCES:=$(filter %.c,$(CODEFILES))
CPPSOURCES:=$(filter %.cpp,$(CODEFILES))

COBJECTS:=$(CSOURCES:%.c=%.c.o)
COBJECTS+=$(CPPSOURCES:%.cpp=%.cpp.o)

# ASOURCES:=$(wildcard kernel/src/*.asm)
ASOURCES:=$(filter %.asm,$(CODEFILES))
AOBJECTS:=$(ASOURCES:%.asm=%.ao)

KERNEL=build/kernel.elf

AFLAGS=-f elf64 -w-zext-reloc

CINCLUDES=-I./include/



COMMON_FLAGS := $(CINCLUDES) -fno-omit-frame-pointer \
				 -Wno-unused-functions \
				 -Wno-sign-compare\
			   -ffreestanding \
			   -fno-stack-protector \
			   -fno-strict-aliasing \
         -fno-strict-overflow \
			   -mno-red-zone \
			   -mcmodel=large -O3 -fno-tree-vectorize \
				 -DGITREVISION=$(shell git rev-parse --short HEAD)

CFLAGS:= $(COMMON_FLAGS) -Wall -fno-common -Wstrict-overflow=5

CPPFLAGS:= -std=c++17 -fno-rtti -fno-exceptions -fno-omit-frame-pointer

DFLAGS=-g -DDEBUG -O0



default: iso

build:
	@mkdir -p build

kern: build $(KERNEL)

%.c.o: %.c
	@echo " CC " $<
	@$(CC) $(CFLAGS) -o $@ -c $<

%.cpp.o: %.cpp
	@echo " CX " $<
	@$(CXX) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<


%.ao: %.asm
	@echo " AS " $<
	@$(AS) $(AFLAGS) -o $@ $<

$(KERNEL): build/fs.img $(CODEFILES) $(ASOURCES) $(COBJECTS) $(AOBJECTS)
	@echo " LD " $@
	@$(LD) $(LDFLAGS) $(AOBJECTS) $(COBJECTS) -T kernel.ld -o $@


build/fs.img:
	dd if=/dev/urandom of=$@ bs=512M count=1
	chmod 666 $@
	mkfs.ext2 $@
	mkdir -p build/mnt
	sudo mount -o loop build/fs.img build/mnt
	sudo cp -r mnt/. build/mnt/
	sudo cp -r src build/mnt/kernel
	sudo umount build/mnt

fs:
	rm -rf build/fs.img
	make build/fs.img

iso: kern
	mkdir -p build/iso/boot/grub
	cp ./grub.cfg build/iso/boot/grub
	cp build/kernel.elf build/iso/boot
	grub-mkrescue -o build/kernel.iso build/iso


klean:
	rm -rf build/fs.img
	rm -rf $(COBJECTS)
	rm -rf $(AOBJECTS)
	rm -rf $(KERNEL)
	rm -rf $(shell find kernel | grep "\.o\$$")
	rm -rf $(shell find kernel | grep "\.ao\$$")


clean: klean
	rm -rf $(shell find . -type f | grep "mobo\$$")
	rm -rf $(shell find . | grep "\.o\$$")
	rm -rf build


qemu: iso
	@qemu-system-x86_64 \
		-serial stdio \
		-cdrom build/kernel.iso \
		-m 8G \
		-drive file=build/fs.img,if=ide

qemu-nox: iso
	@qemu-system-x86_64 \
		-nographic \
		-cdrom build/kernel.iso \
		-m 8G \
		-drive file=build/fs.img,if=ide


bochs: iso
	@bochs -f bochsrc
