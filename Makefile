CC = gcc
AS = nasm
LD = ld



STRUCTURE := $(shell find kernel/src -type d)
CODEFILES := $(addsuffix /*,$(STRUCTURE))
CODEFILES := $(wildcard $(CODEFILES))


CSOURCES:=$(filter %.c,$(CODEFILES))
# CSOURCES:=$(wildcard kernel/src/*.c)
COBJECTS:=$(CSOURCES:%.c=%.o)

# ASOURCES:=$(wildcard kernel/src/*.asm)
ASOURCES:=$(filter %.asm,$(CODEFILES))
AOBJECTS:=$(ASOURCES:%.asm=%.ao)

KERNEL=build/kernel.elf

AFLAGS=-f elf64 -w-zext-reloc

CINCLUDES=-Ikernel/include/



COMMON_FLAGS := $(CINCLUDES) -fno-omit-frame-pointer \
			   -ffreestanding \
			   -fno-stack-protector \
			   -fno-strict-aliasing \
         -fno-strict-overflow \
			   -mno-red-zone \
			   -mcmodel=large -O3 -fno-tree-vectorize

CFLAGS:=   $(COMMON_FLAGS) -Wall -fno-common -Wstrict-overflow=5

DFLAGS=-g -DDEBUG -O0



# COMMON_FLAGS := -nostdlib -nostdinc -fno-builtin -fno-stack-protector -Wall -Wextra -c
# CFLAGS := $(COMMON_FLAGS) -Wall -static -fno-common -Wstrict-overflow=5

# By default, build the example kernel using make and the vmm using cmake
default: build $(KERNEL)
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j --no-print-directory


kern: build $(KERNEL)

build:
	@mkdir -p build



%.o: %.c
	@echo " CC " $<
	@$(CC) $(CFLAGS) -o $@ -c $<


%.ao: %.asm
	@echo " AS " $<
	@$(AS) $(AFLAGS) -o $@ $<

$(KERNEL): $(CSOURCES) $(ASOURCES) $(COBJECTS) $(AOBJECTS)
	@echo " LD " $@
	@$(LD) $(LDFLAGS) $(AOBJECTS) $(COBJECTS) -T kernel/kernel.ld -o $@


iso: kern
	mkdir -p build/iso/boot/grub
	cp kernel/grub.cfg build/iso/boot/grub
	cp build/kernel.elf build/iso/boot
	grub-mkrescue -o kernel.iso build/iso


klean:
	rm -r $(KERNEL)
	rm -f $(COBJECTS)
	rm -f $(AOBJECTS)

clean: klean
	rm -rf build


qemu: iso
		qemu-system-x86_64 \
			-nographic \
			-cdrom kernel.iso \
			-m 2048 \
