CC = $(X86_64_ELF_TOOLCHAIN)gcc$(TOOLCHAIN_GCC_VERSION)
CXX = $(X86_64_ELF_TOOLCHAIN)g++$(TOOLCHAIN_GCC_VERSION)
AS = nasm
LD = $(X86_64_ELF_TOOLCHAIN)ld
GRUB = $(GRUB_PREFIX)grub-mkrescue

.PHONY: fs watch

include Makefile.common

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

KERNEL=build/vmchariot
ISO=build/kernel.iso
SYMS=build/kernel.syms
ROOTFS=build/root.img



default: $(KERNEL)

build:
	@mkdir -p build


build/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) $(CC)  " $<
	@$(CC) $(CFLAGS) -o $@ -c $<

build/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) $(CXX) " $<
	@$(CXX) $(CPPFLAGS) -o $@ -c $<


build/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) ASM " $<
	@$(AS) $(AFLAGS) -o $@ $<

$(KERNEL): $(CODEFILES) $(ASOURCES) $(COBJECTS) $(AOBJECTS)
	@echo -e "$(PFX) LNK " $@
	@$(LD) $(LDFLAGS) $(AOBJECTS) $(COBJECTS) -T kernel.ld -o $@


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
	cp build/vmchariot build/iso/boot
	cp build/kernel.syms build/iso/boot
	$(GRUB) -o $(ISO) build/iso


klean:
	rm -f $(COBJECTS) build/initrd.tar $(AOBJECTS) $(KERNEL)

clean:
	rm -rf user/out user/build
	rm -rf build

images: $(ROOTFS)

QEMUOPTS=-hda $(ROOTFS) -smp 4 -m 2G -gdb tcp::8256

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



watch:
	while inotifywait **/*; do ./sync.sh; done
