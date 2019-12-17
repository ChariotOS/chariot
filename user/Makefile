include ../Makefile.common


DEFINES += -DUSERLAND

OPTIMIZE:=-O3

CFLAGS:=$(CINCLUDES) $(OPTIMIZE) $(MOREFLAGS) -fno-omit-frame-pointer -fno-stack-protector \
				 -mtls-direct-seg-refs -ffreestanding -Wall \


.PHONY: bin default setup

LDFLAGS += -L$(PWD)/lib/



srcs:=$(DIR)


-include $(DIR)/Makefile



srcs = $(shell find $(DIR))

CSOURCES = $(filter %.c,$(srcs))
COBJECTS:=$(CSOURCES:%.c=bin/%.c.o)


CPPSOURCES = $(filter %.cpp,$(srcs))
CPPOBJECTS:=$(CPPSOURCES:%.cpp=bin/%.cpp.o)


ASOURCES = $(filter %.asm,$(srcs))
AOBJECTS:=$(ASOURCES:%.asm=bin/%.asm.o)


setup:


bin:
	@mkdir -p bin

$(BIN): lib/crt0.o $(COBJECTS) $(CPPOBJECTS) $(AOBJECTS)
	@echo "sources: $(srcs)"
	@echo "[U] LD   $(BIN)   $(ULDFLAGS)"
	@$(LD) $(LDFLAGS) -o $(BIN) lib/crt0.o $(CPPOBJECTS) $(COBJECTS) $(AOBJECTS) -lc $(ULDFLAGS)
	@echo ""


$(LIB): $(COBJECTS) $(CPPOBJECTS) $(AOBJECTS)
	@echo "[U] AR   $(LIB)"
	@ar rcs $(LIB) $(COBJECTS) $(AOBJECTS)
	@echo ""

prog: setup $(BIN)

lib: setup $(LIB)

shlib: $(COBJECTS) $(CPPOBJECTS) $(AOBJECTS)
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
	@echo "[U] CC  " $<
	@$(CC) $(CFLAGS) -o $@ -c $<


bin/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[U] CXX " $<
	@$(CXX) $(CFLAGS) -std=c++17 -fno-rtti -fno-exceptions -o $@ -c $<


bin/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	@echo "[U] AS  " $<
	@nasm -f elf64 -o $@ $<


clean:
	@rm -rf lib
	@rm -rf bin
