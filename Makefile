.PHONY: watch

export PATH := $(PWD)/toolchain/local/bin/:$(PATH)
TOOLCHAIN=x86_64-elf-chariot-
include Makefile.common

AFLAGS=-f elf64 -w-zext-reloc -D __ARCH_$(ARCH)__

CINCLUDES=-I./include/

CFLAGS=$(CINCLUDES) $(DFLAGS) -D__ARCH_$(ARCH)__ -mno-red-zone                   \
           -fno-omit-frame-pointer -fno-stack-protector                           \
           -mtls-direct-seg-refs -fno-pie -Wno-sign-compare -ffreestanding        \
           -mcmodel=large $(OPT) -Wall -fno-common -Wno-initializer-overrides     \
           -Wstrict-overflow=5 -fno-tree-vectorize -Wno-address-of-packed-member  \
           -Wno-strict-overflow -DGIT_REVISION=\"$(shell git rev-parse HEAD)\" -DKERNEL

CPPFLAGS = $(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions

BINDIR=build

default:
	@mkdir -p build
	@if [ ! -d build/kernel ]; then  cd build; cmake -GNinja ../ ; fi
	@cd build; ninja install # make install --quiet --no-print-directory -j
	@cp build/compile_commands.json .

clean:
	@rm -rf build

