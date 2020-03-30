.PHONY: fs watch

ROOTDIR = .
include Makefile.common



LDFLAGS=-m elf_x86_64
AFLAGS=-f elf64 -w-zext-reloc -D __ARCH_$(ARCH)__

CINCLUDES=-I./include/

CFLAGS:=$(CINCLUDES) $(DFLAGS) -D__ARCH_$(ARCH)__ -mno-red-zone -fno-omit-frame-pointer -fno-stack-protector \
				 -mtls-direct-seg-refs -fno-pie -Wno-sign-compare -ffreestanding \
				 -mcmodel=large $(OPT) -Wall -fno-common -Wno-initializer-overrides \
				 -Wstrict-overflow=5 -fno-tree-vectorize -Wno-address-of-packed-member \
				 -Wno-strict-overflow -DGIT_REVISION=\"$(shell git rev-parse HEAD)\"

CPPFLAGS:=$(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions

STRUCTURE := $(shell tools/get_structure.sh $(ARCH))
CODEFILES := $(addsuffix /*,$(STRUCTURE))
CODEFILES := $(wildcard $(CODEFILES))


CSOURCES:=$(filter %.c,$(CODEFILES))
CPPSOURCES:=$(filter %.cpp,$(CODEFILES))

COBJECTS:=$(CSOURCES:%.c=build/%.c.o)
COBJECTS+=$(CPPSOURCES:%.cpp=build/%.cpp.o)

# ASOURCES:=$(wildcard kernel/src/*.asm)
ASOURCES:=$(filter %.asm,$(CODEFILES))
AOBJECTS:=$(ASOURCES:%.asm=build/%.asm.o)


PFX:="\\x1b[32m[K]\\x1b[0m"

KERNEL=build/chariot.elf
ISO=build/kernel.iso
SYMS=build/kernel.syms
ROOTFS=build/root.img



default: $(KERNEL)

build:
	@mkdir -p build


build/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) CC   " $<
	@$(CC) $(CFLAGS) -o $@ -c $<

build/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) CXX " $<
	@$(CXX) $(CPPFLAGS) -o $@ -c $<


build/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) ASM " $<
	@$(AS) $(AFLAGS) -o $@ $<

$(KERNEL): $(CODEFILES) $(ASOURCES) $(COBJECTS) $(AOBJECTS)
	@echo -e "$(PFX) LNK " $@
	@$(LD) $(LDFLAGS) $(AOBJECTS) $(COBJECTS) -T kernel/kernel.ld -o $@


$(SYMS): $(KERNEL)
	nm -s $< | c++filt | cut -c -80 > $@

build/initrd.tar: build
	@#tar cvf $@ mnt


$(ROOTFS): $(KERNEL)
	./sync.sh

fs:
	rm -rf $(ROOTFS)
	make $(ROOTFS)

$(ISO): $(KERNEL) grub.cfg
	mkdir -p build/iso/boot/grub
	cp ./grub.cfg build/iso/boot/grub
	cp build/chariot.elf build/iso/boot
	cp build/kernel.syms build/iso/boot
	$(GRUB) -o $(ISO) build/iso


klean:
	rm -f $(COBJECTS) build/initrd.tar $(AOBJECTS) $(KERNEL)

clean:
	rm -rf user/out user/build
	rm -rf build

images: $(ROOTFS)


gdb:
	gdb $(KERNEL) -iex "target remote localhost:8256"


watch:
	while inotifywait **/*; do ./sync.sh; done
