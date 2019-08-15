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

COBJECTS:=$(CSOURCES:%.c=build/%.c.o)
COBJECTS+=$(CPPSOURCES:%.cpp=build/%.cpp.o)

# ASOURCES:=$(wildcard kernel/src/*.asm)
ASOURCES:=$(filter %.asm,$(CODEFILES))
AOBJECTS:=$(ASOURCES:%.asm=build/%.asm.o)

KERNEL=build/kernel.elf
ROOTFS=build/root.img

AFLAGS=-f elf64 -w-zext-reloc

CINCLUDES=-I./include/



COMMON_FLAGS := $(CINCLUDES)  \
				 -fPIC \
				 -Wno-sign-compare\
			   -ffreestanding \
			   -mno-red-zone \
			   -mcmodel=large -O3 -fno-tree-vectorize \
				 -DGIT_REVISION=\"$(shell git rev-parse HEAD)\"

CFLAGS:= $(COMMON_FLAGS) -Wall -fno-common -Wstrict-overflow=5

CPPFLAGS:= -std=c++17 -fno-rtti -fno-exceptions -fno-omit-frame-pointer

DFLAGS=-g -DDEBUG -O0




default: $(KERNEL)

build:
	@mkdir -p build


build/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo " CC  " $<
	@$(CC) $(CFLAGS) -o $@ -c $<

build/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	@echo " CXX " $<
	@$(CXX) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<


build/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo " ASM " $<
	@$(AS) $(AFLAGS) -o $@ $<

$(KERNEL): $(CODEFILES) $(ASOURCES) $(COBJECTS) $(AOBJECTS)
	@echo " LNK " $@
	@$(LD) $(LDFLAGS) $(AOBJECTS) $(COBJECTS) -T kernel.ld -o $@


$(ROOTFS):
	dd if=/dev/urandom of=$@ bs=512M count=1
	chmod 666 $@
	mkfs.ext2 $@
	mkdir -p build/mnt
	sudo mount -o loop $(ROOTFS) build/mnt
	sudo cp -r mnt/. build/mnt/
	sudo cp -r src build/mnt/kernel
	sudo umount build/mnt

fs:
	rm -rf $(ROOTFS)
	make $(ROOTFS)

iso: $(KERNEL)
	mkdir -p build/iso/boot/grub
	cp ./grub.cfg build/iso/boot/grub
	cp build/kernel.elf build/iso/boot
	grub-mkrescue -o build/kernel.iso build/iso


clean:
	rm -rf build


QEMUOPTS=-cdrom build/kernel.iso\
				 -m 6G \
				 -hda $(ROOTFS)

qemu: iso $(ROOTFS)
	qemu-system-x86_64 $(QEMUOPTS) \
		-serial stdio

qemu-nox: iso $(ROOTFS)
	qemu-system-x86_64 $(QEMUOPTS) \
		-nographic


bochs: iso
	@bochs -f bochsrc
