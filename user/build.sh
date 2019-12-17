#!/usr/bin/env bash

PROGS=$(ls src/bin)
LIBS=$(ls src/lib)


mkdir -p bin
mkdir -p lib

for lib in $LIBS; do
	LIB=lib/$lib.a
	make lib DIR=src/lib/$prog LIB=$LIB # MOREFLAGS=-fpic
done


for prog in $PROGS; do
	BIN=bin/$prog
	make prog DIR=src/bin/$prog BIN=$BIN ULDFLAGS=$(cat src/bin/$prog/ld_flags.txt 2>/dev/null)
done
