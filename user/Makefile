
# include ../Makefile.common

SRCS = $(wildcard *.c)
APPS = ${SRCS:.c=}

DEFINES += -DUSERLAND

.PHONY: all list clean

all: $(OBJS) $(APPS)

clean:
	@echo "CLEAN"
	@rm -f $(APPS) $(OBJS) *.d

$(APPS) : % : %.c
	echo "CC $@"
	gcc -o $@ -ffreestanding $<


