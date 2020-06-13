.PHONY: run

export PATH := $(PWD)/../toolchain/local/bin/:$(PATH)
TOOLCHAIN=x86_64-elf-chariot-
include ../Makefile.common


CINCLUDES=
DEFINES += -DUSERLAND
OPTIMIZE:=-O3

CFLAGS  = -I./include/ $(MOREFLAGS) -fPIE
CFLAGS += -ffreestanding -Wall
CFLAGS += $(DEFINES)
CPPFLAGS = $(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions


DST:=../base
BUILD:=../build/userspace

LDFLAGS += -L$(DST)/lib/

libs = -lc


-include $(M)/Makefile

PFX:="\\x1b[34m[U]\\x1b[0m"

ifdef M
ifndef srcs
$(error srcs not provided in program)
endif
ifndef out
$(error output not defined)
endif
endif

# reify paths
srcs:=$(srcs:%=$(M)/%)



# fix up $out to be relative to the build destination
out:=$(DST)/$(out)
outdir := $(dir $(out))

# build a list of object files
objs:=$(srcs:%=$(BUILD)/%.o)


# uncomment to enable PIE code
# LDFLAGS += -pie -z combreloc -z separate-code


# disable make's intermediate file removal
.PRECIOUS: $(objs)


$(BUILD):
	@mkdir -p $(BUILD)


run: $(BUILD) $(out)
	@#echo -e "Done\n"



# binary programs
$(DST)/bin/%: $(BUILD)/crt0.o $(objs)
	@mkdir -p $(outdir)
	@echo -e "$(PFX) LD   $(M) with $(libs)"
	@$(LD) -T user.ld $(LDFLAGS) -o $@ $^ $(libs)


# static library archives
$(DST)/lib/%.a: $(objs)
	@mkdir -p $(outdir)
	@echo -e "$(PFX) AR   $@"
	@ar rcs $@ $^

# dynamic library objects
# out/lib/%.so:
#	$(error dynamic libraries cannot be built)

$(BUILD)/crt0.o: crt0.asm
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) making crt0.o"
	@nasm -f elf64 -o $@ $^

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) CC  " $<
	@$(CC) $(CFLAGS) -o $@ -c $<

$(BUILD)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) CXX " $<
	@$(CXX) $(CPPFLAGS) -o $@ -c $<



$(BUILD)/%.rs.o: %.rs
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) RS  " $<
	rustc -O -C target-feature=+crt-static  \
		--edition 2018                        \
		--crate-type=bin                      \
		-L crate=crates/                      \
		-L out/lib/rs -l static=std           \
		--emit obj -o $@ $<


$(BUILD)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) AS  " $<
	@nasm -f elf64 -o $@ $<

clean:
	rm -rf $(DST)/bin $(DST)/lib $(BUILD)

