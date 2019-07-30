CC = gcc
AS = nasm
LD = ld




VMM_CPP_FILES:=$(shell find src | grep "\.cpp")
VMM_HS_FILES:=$(shell find src | grep "\.hs")
VMM_INCLUDE_FILES:=$(shell find include)


FILEDEPS:=$(VMM_CPP_FILES) $(VMM_HS_FILES) $(VMM_INCLUDE_FILES)


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
			   -mcmodel=large -O0 -fno-tree-vectorize

CFLAGS:= $(COMMON_FLAGS) -Wall -fno-common -Wstrict-overflow=5

DFLAGS=-g -DDEBUG -O0



# By default, build the example kernel using make and the vmm using stack
default: build $(KERNEL) $(FILEDEPS)
	@stack build
	@cp $$(find . | grep "/bin/mobo") build/


fast: build $(KERNEL) $(FILEDEPS)
	@stack build --fast
	@cp $$(find . | grep "/bin/mobo") build/

build:
	@mkdir -p build






# test kernel related build tools
kern: build $(KERNEL)


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
	rm -rf $(shell find . -type f | grep "mobo\$$")
	rm -rf $(shell find . | grep "\.o\$$")
	rm -rf build


qemu: iso
		qemu-system-x86_64 \
			-nographic \
			-cdrom kernel.iso \
			-m 2048 \
