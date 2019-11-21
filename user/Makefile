
FILES = $(shell find $(DIR))

CSOURCES = $(filter %.c,$(FILES))
COBJECTS:=$(CSOURCES:%.c=bin/%.c.o)


ASOURCES = $(filter %.asm,$(FILES))
AOBJECTS:=$(ASOURCES:%.asm=bin/%.asm.o)

DEFINES += -DUSERLAND

.PHONY: bin default


bin:
	@mkdir -p bin

$(BIN): $(COBJECTS) $(AOBJECTS)
	@echo " LD  $(BIN)"
	@$(LD) -o $(BIN) $(LDFLAGS) $<


all:
	./build.sh




bin/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo " CC " $<
	@$(CC) -o $@ -c $<



bin/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo " AS " $<
	@nasm -f elf64 -o $@  $<

clean:
	@rm -rf bin
