.PHONY: run

include ../Makefile.common


CINCLUDES=
DEFINES += -DUSERLAND
OPTIMIZE:=-O3

CFLAGS  = -I./include/ $(MOREFLAGS) -fPIE
CFLAGS += -ffreestanding -Wall
CFLAGS += $(DEFINES)
CPPFLAGS = $(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions


LDFLAGS += -L$(PWD)/out/lib/

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

DST=out/

# reify paths
srcs:=$(srcs:%=$(M)/%)

# fix up $out to be relative to the build destination
out:=out/$(out)
outdir := $(dir $(out))

# build a list of object files
objs:=$(srcs:%=build/%.o)


# uncomment to enable PIE code
# LDFLAGS += -pie -z combreloc -z separate-code


# disable make's intermediate file removal
.PRECIOUS: $(objs)

out:
	@mkdir -p out
	@mkdir -p out/bin
	@mkdir -p out/lib
	@mkdir -p $(dir $(out))

build:
	@mkdir -p build


run: build out $(out)
	@#echo -e "Done\n"



# binary programs
out/bin/%: build/crt0.o $(objs)
	@mkdir -p $(outdir)
	@echo -e "$(PFX) LD   $@ with $(libs)"
	@$(LD) -T user.ld $(LDFLAGS) -o $@ $^ $(libs)

# static library archives
out/lib/%.a: $(objs)
	@mkdir -p $(outdir)
	@echo -e "$(PFX) AR   $@"
	@ar rcs $@ $^

# dynamic library objects
# out/lib/%.so:
#	$(error dynamic libraries cannot be built)

build/crt0.o: crt0.asm
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) making crt0.o"
	@nasm -f elf64 -o $@ $^

build/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) CC  " $<
	@$(CC) $(CFLAGS) -o $@ -c $<

build/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) CXX " $<
	@$(CXX) $(CPPFLAGS) -std=c++17 -fno-rtti -fno-exceptions -o $@ -c $<



build/%.rs.o: %.rs
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) RS  " $<
	rustc -O -C target-feature=+crt-static  \
		--edition 2018                        \
		--crate-type=bin                      \
		-L crate=crates/                      \
		-L out/lib/rs -l static=std           \
		--emit obj -o $@ $<


build/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo -e "$(PFX) AS  " $<
	@nasm -f elf64 -o $@ $<

clean:
	rm -rf out build

