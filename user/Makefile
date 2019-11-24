include ../Makefile.common

FILES = $(shell find $(DIR))

CSOURCES = $(filter %.c,$(FILES))
COBJECTS:=$(CSOURCES:%.c=bin/%.c.o)


ASOURCES = $(filter %.asm,$(FILES))
AOBJECTS:=$(ASOURCES:%.asm=bin/%.asm.o)

DEFINES += -DUSERLAND



.PHONY: bin default

LDFLAGS += -L$(PWD)/lib/


bin:
	@mkdir -p bin

$(BIN): $(COBJECTS) $(AOBJECTS)
	@echo " LD  $(BIN)   $(ULDFLAGS)"
	@$(LD) $(LDFLAGS) -o $(BIN) $(COBJECTS) $(AOBJECTS) -lc $(ULDFLAGS)
	@echo ""


$(LIB): $(COBJECTS) $(AOBJECTS)
	@echo " AR  $(LIB)"
	@ar rcs $(LIB) $(COBJECTS) $(AOBJECTS)
	@echo ""

prog: $(BIN)

lib: $(LIB)

all:
	./build.sh


bin/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo " CC " $<
	@$(CC) $(CFLAGS) -o $@ -c $<


bin/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo " AS " $<
	@nasm -f elf64 -o $@  $<


clean:
	@rm -rf lib
	@rm -rf bin
