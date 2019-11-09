SRCS = $(wildcard *.c)
APPS = ${SRCS:.c=}


ASMSRCS = $(wildcard *.asm)
APPS += ${ASMSRCS:.asm=}

DEFINES += -DUSERLAND

.PHONY: all list clean

all: $(OBJS) $(APPS)

clean:
	@echo "CLEAN"
	@rm -f $(APPS) $(OBJS) *.d


$(APPS) : % : %.asm
	@echo "ASM $@"
	nasm -f bin -o $@ $<

