CC = $(X86_64_ELF_TOOLCHAIN)gcc
CXX = $(X86_64_ELF_TOOLCHAIN)g++
AS = nasm
LD = $(X86_64_ELF_TOOLCHAIN)ld

GRUB = $(GRUB_PREFIX)grub-mkrescue

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
				 -O3 -fno-tree-vectorize \
				 -DGIT_REVISION=\"$(shell git rev-parse HEAD)\"


CFLAGS:=$(COMMON_FLAGS) -Wall -fno-common -Wstrict-overflow=5

CPPFLAGS:=$(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions -fno-omit-frame-pointer

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
	@$(CXX) $(CPPFLAGS) -o $@ -c $<


build/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo " ASM " $<
	@$(AS) $(AFLAGS) -o $@ $<

$(KERNEL): build/initrd.tar $(CODEFILES) $(ASOURCES) $(COBJECTS) $(AOBJECTS)
	@echo " LNK " $@
	@$(LD) $(LDFLAGS) $(AOBJECTS) $(COBJECTS) -T kernel.ld -o $@


build/initrd.tar: build
	@#tar cvf $@ mnt


$(ROOTFS):
	dd if=/dev/urandom of=$@ bs=1M count=30
	chmod 666 $@
	mkfs.ext2 $@
	mkdir -p build/mnt
	sudo mount -o loop $(ROOTFS) build/mnt
	sudo cp -r mnt/. build/mnt/
	sudo cp -r src build/mnt/src
	sudo cp -r include build/mnt/include
	sudo umount build/mnt

fs:
	rm -rf $(ROOTFS)
	make $(ROOTFS)

iso: $(KERNEL)
	mkdir -p build/iso/boot/grub
	cp ./grub.cfg build/iso/boot/grub
	cp build/kernel.elf build/iso/boot
	$(GRUB) -o build/kernel.iso build/iso


klean:
	rm -f $(COBJECTS) build/initrd.tar $(AOBJECTS) $(KERNEL)

clean:
	rm -rf build


QEMUOPTS=-hda build/kernel.iso -smp 4 -m 8G -hdb $(ROOTFS)

qemu: iso $(ROOTFS)
	qemu-system-x86_64 $(QEMUOPTS) \
		-serial stdio

qemu-nox: iso $(ROOTFS)
	qemu-system-x86_64 $(QEMUOPTS) \
		-nographic


bochs: iso
	@bochs -f bochsrc
