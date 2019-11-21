CC = $(X86_64_ELF_TOOLCHAIN)gcc
CXX = $(X86_64_ELF_TOOLCHAIN)g++
AS = nasm
LD = $(X86_64_ELF_TOOLCHAIN)ld
GRUB = $(GRUB_PREFIX)grub-mkrescue

.PHONY: fs


STRUCTURE := $(shell tools/get_structure.sh)
CODEFILES := $(addsuffix /*,$(STRUCTURE))
CODEFILES := $(wildcard $(CODEFILES))


CSOURCES:=$(filter %.c,$(CODEFILES))
CPPSOURCES:=$(filter %.cpp,$(CODEFILES))

COBJECTS:=$(CSOURCES:%.c=build/%.c.o)
COBJECTS+=$(CPPSOURCES:%.cpp=build/%.cpp.o)

# ASOURCES:=$(wildcard kernel/src/*.asm)
ASOURCES:=$(filter %.asm,$(CODEFILES))
AOBJECTS:=$(ASOURCES:%.asm=build/%.asm.o)


LDFLAGS=-m elf_x86_64

KERNEL=build/vmchariot
ISO=build/kernel.iso
SYMS=build/kernel.syms
ROOTFS=build/root.img

AFLAGS=-f elf64 -w-zext-reloc

CINCLUDES=-I./include/



DFLAGS=-g -DDEBUG -O3

CFLAGS:=$(CINCLUDES) -mno-red-zone -fno-omit-frame-pointer -fno-stack-protector \
				 -mtls-direct-seg-refs -fno-pie -Wno-sign-compare -ffreestanding \
				 -mcmodel=large -O3 -Wall -fno-common -Wno-initializer-overrides \
				 -Wstrict-overflow=5 -fno-tree-vectorize -Wno-address-of-packed-member \
				 -Wno-strict-overflow -DGIT_REVISION=\"$(shell git rev-parse HEAD)\"

# CLFAGS+=$(DFLAGS)




CPPFLAGS:=$(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions




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

$(KERNEL): $(CODEFILES) $(ASOURCES) $(COBJECTS) $(AOBJECTS)
	@echo " LNK " $@
	@$(LD) $(LDFLAGS) $(AOBJECTS) $(COBJECTS) -T kernel.ld -o $@


$(SYMS): $(KERNEL)
	nm -s $< > $@

build/initrd.tar: build
	@#tar cvf $@ mnt


$(ROOTFS):
	./sync.sh

fs:
	rm -rf $(ROOTFS)
	make $(ROOTFS)

$(ISO): $(KERNEL) $(SYMS) grub.cfg
	mkdir -p build/iso/boot/grub
	cp ./grub.cfg build/iso/boot/grub
	cp build/vmchariot build/iso/boot
	cp build/kernel.syms build/iso/boot
	$(GRUB) -o $(ISO) build/iso


klean:
	rm -f $(COBJECTS) build/initrd.tar $(AOBJECTS) $(KERNEL)

clean:
	rm -rf user/bin
	rm -rf build



images: $(ISO)
	./sync.sh

QEMUOPTS=-hda $(ISO) -m 2G -hdb $(ROOTFS) -gdb tcp::8256

qemu: images
	qemu-system-x86_64 $(QEMUOPTS) \
		-serial stdio

qemu-nox: images
	qemu-system-x86_64 $(QEMUOPTS) -nographic



qemu-dbg: images
	qemu-system-x86_64 $(QEMUOPTS) -d cpu_reset

gdb:
	gdb $(KERNEL) -iex "target remote localhost:8256"



bochs: $(ISO)
	@bochs -f bochsrc
