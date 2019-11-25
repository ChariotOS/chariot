include ../Makefile.common

FILES = $(shell find $(DIR))

CSOURCES = $(filter %.c,$(FILES))
COBJECTS:=$(CSOURCES:%.c=bin/%.c.o)


ASOURCES = $(filter %.asm,$(FILES))
AOBJECTS:=$(ASOURCES:%.asm=bin/%.asm.o)

DEFINES += -DUSERLAND

OPTIMIZE:=-O3

CFLAGS:=$(CINCLUDES) $(OPTIMIZE) -fno-omit-frame-pointer -fno-stack-protector \
				 -mtls-direct-seg-refs -ffreestanding -Wall -Wstrict-overflow=5 \


.PHONY: bin default

LDFLAGS += -L$(PWD)/lib/


bin:
	@mkdir -p bin

$(BIN): $(COBJECTS) $(AOBJECTS)
	@echo "[U] LD  $(BIN)   $(ULDFLAGS)"
	@$(LD) $(LDFLAGS) -o $(BIN) $(COBJECTS) $(AOBJECTS) -lc $(ULDFLAGS)
	@echo ""


$(LIB): $(COBJECTS) $(AOBJECTS)
	@echo "[U] AR  $(LIB)"
	@ar rcs $(LIB) $(COBJECTS) $(AOBJECTS)
	@echo ""

prog: $(BIN)

lib: $(LIB)

all:
	./build.sh


bin/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo "[U] CC " $<
	@$(CC) $(CFLAGS) -o $@ -c $<


bin/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo "[U] AS " $<
	@nasm -f elf64 -o $@  $<


clean:
	@rm -rf lib
	@rm -rf bin
