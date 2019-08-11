CC = gcc
CXX = g++
AS = nasm
LD = ld


.PHONY: fs

VMM_CPP_FILES:=$(shell find src | grep "\.cpp")
VMM_HS_FILES:=$(shell find src | grep "\.hs")
VMM_INCLUDE_FILES:=$(shell find include)


FILEDEPS:=$(VMM_CPP_FILES) $(VMM_HS_FILES) $(VMM_INCLUDE_FILES)


STRUCTURE := $(shell find kernel -type d)
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

CINCLUDES=-Ikernel/include/



COMMON_FLAGS := $(CINCLUDES) -fno-omit-frame-pointer \
				 -Wno-unused-functions \
				 -Wno-sign-compare\
			   -ffreestanding \
				 -fpermissive \
			   -fno-stack-protector \
			   -fno-strict-aliasing \
         -fno-strict-overflow \
			   -mno-red-zone \
			   -mcmodel=large -O0 -fno-tree-vectorize

CFLAGS:= $(COMMON_FLAGS) -Wall -fno-common -Wstrict-overflow=5

CPPFLAGS:= -std=c++17 -fno-rtti -fno-exceptions -fno-omit-frame-pointer

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
	@$(LD) $(LDFLAGS) $(AOBJECTS) $(COBJECTS) -T kernel/kernel.ld -o $@


build/fs.img:
	dd if=/dev/urandom of=$@ bs=512M count=1
	chmod 666 $@
	mkfs.ext2 $@
	mkdir -p build/mnt
	sudo mount -o loop build/fs.img build/mnt
	sudo cp -r mnt/. build/mnt/
	sudo umount build/mnt

fs:
	rm -rf build/fs.img
	make build/fs.img

iso: kern
	mkdir -p build/iso/boot/grub
	cp kernel/grub.cfg build/iso/boot/grub
	cp build/kernel.elf build/iso/boot
	grub-mkrescue -o build/kernel.iso build/iso


klean:
	rm -rf build/fs.img
	rm -rf $(COBJECTS)
	rm -rf $(AOBJECTS)
	rm -rf $(shell find kernel | grep "\.o\$$")
	rm -r $(KERNEL)


clean: klean
	rm -rf $(shell find . -type f | grep "mobo\$$")
	rm -rf $(shell find . | grep "\.o\$$")
	rm -rf build


qemu: iso
		@qemu-system-x86_64 \
			-nographic \
			-cdrom build/kernel.iso \
			-m 8G \
			-drive file=build/fs.img,if=ide
