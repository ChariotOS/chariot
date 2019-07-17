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
CWARNINGS=-Wall -Wextra
CFLAGS=-fno-stack-protector -nostdlib -fpermissive -nostartfiles -nodefaultlibs -nostdinc -ffreestanding -fno-builtin $(CWARNINGS) $(CINCLUDES)
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
	cp grub.cfg build/iso/boot/grub
	cp build/kernel.elf build/iso/boot
	grub-mkrescue -o kernel.iso build/iso

clean:
	@rm -rf build
	@rm -f $(COBJECTS)
	@rm -f $(AOBJECTS)
