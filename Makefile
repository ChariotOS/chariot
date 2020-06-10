.PHONY: watch

export PATH := $(PWD)/toolchain/local/bin/:$(PATH)
TOOLCHAIN=x86_64-elf-chariot-
include Makefile.common

LDFLAGS=-m elf_x86_64
AFLAGS=-f elf64 -w-zext-reloc -D __ARCH_$(ARCH)__

CINCLUDES=-I./include/

CFLAGS=$(CINCLUDES) $(DFLAGS) -D__ARCH_$(ARCH)__ -mno-red-zone                   \
           -fno-omit-frame-pointer -fno-stack-protector                           \
           -mtls-direct-seg-refs -fno-pie -Wno-sign-compare -ffreestanding        \
           -mcmodel=large $(OPT) -Wall -fno-common -Wno-initializer-overrides     \
           -Wstrict-overflow=5 -fno-tree-vectorize -Wno-address-of-packed-member  \
           -Wno-strict-overflow -DGIT_REVISION=\"$(shell git rev-parse HEAD)\" -DKERNEL

CPPFLAGS = $(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions

src-y =



# load the config file
include config.mk


# and all the makefiles to kick off source discovery
include kernel/Makefile
include drivers/Makefile
include arch/$(ARCH)/Makefile
include fs/Makefile
include net/Makefile

objs = $(src-y:%=build/%.o)


PFX:="\\x1b[32m[K]\\x1b[0m"

BIN=build/chariot.elf

default: $(BIN)


build/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) CC  " $<
	@$(CC) $(CFLAGS) -o $@ -c $<

build/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) CXX " $<
	@$(CXX) $(CPPFLAGS) -o $@ -c $<


build/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) ASM " $<
	@$(AS) $(AFLAGS) -o $@ $<

$(BIN): $(src-y) $(objs)
	@echo -e "$(PFX) LNK " $@
	@$(LD) $(objs) -T kernel/kernel.ld -o $@


klean:
	@rm -f $(objs) $(AOBJECTS) $(BIN)

clean:
	rm -rf user/out user/build
	rm -rf build

gdb:
	gdb $(BIN) -iex "target remote localhost:8256"


watch:
	while inotifywait $(src-y); do ./sync.sh; done




font:
	@mkdir -p build
	tools/convert_font.py base/usr/res/fonts/cherry.bdf build/font.ckf
	xxd -i build/font.ckf > drivers/vga/font.ckf.h
