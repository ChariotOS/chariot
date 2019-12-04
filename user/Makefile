include ../Makefile.common

FILES = $(shell find $(DIR))

CSOURCES = $(filter %.c,$(FILES))
COBJECTS:=$(CSOURCES:%.c=bin/%.c.o)


ASOURCES = $(filter %.asm,$(FILES))
AOBJECTS:=$(ASOURCES:%.asm=bin/%.asm.o)

DEFINES += -DUSERLAND

OPTIMIZE:=-O3

CFLAGS:=$(CINCLUDES) $(OPTIMIZE) $(MOREFLAGS) -fno-omit-frame-pointer -fno-stack-protector \
				 -mtls-direct-seg-refs -ffreestanding -Wall -Wstrict-overflow=5 \


.PHONY: bin default

LDFLAGS += -L$(PWD)/lib/


bin:
	@mkdir -p bin

$(BIN): lib/crt0.o $(COBJECTS) $(AOBJECTS)
	@echo "[U] LD  $(BIN)   $(ULDFLAGS)"
	@$(LD) $(LDFLAGS) -o $(BIN) lib/crt0.o $(COBJECTS) $(AOBJECTS) -lc $(ULDFLAGS)
	@echo ""


$(LIB): $(COBJECTS) $(AOBJECTS)
	@echo "[U] AR  $(LIB)"
	@ar rcs $(LIB) $(COBJECTS) $(AOBJECTS)
	@echo ""

prog: $(BIN)

lib: $(LIB)

shlib: $(COBJECTS) $(AOBJECTS)
	@echo "[U] SO  $(LIB)"
	@gcc -shared -o $(LIB) $(COBJECTS) $(AOBJECTS)
	@echo ""

all:
	./build.sh


lib/crt0.o: src/crt0.asm
	@mkdir -p $(dir $@)
	@echo "[U] making crt0.o"
	@nasm -f elf64 -o lib/crt0.o src/crt0.asm

bin/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo "[U] CC " $<
	@$(CC) $(CFLAGS) -o $@ -c $<


bin/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo "[U] AS " $<
	@nasm -f elf64 -o $@ $<


clean:
	@rm -rf lib
	@rm -rf bin
