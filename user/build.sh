#!/usr/bin/env bash

PROGS=$(ls src/bin)
LIBS=$(ls src/lib)


mkdir -p bin
mkdir -p lib

for lib in $LIBS; do
	LIB=lib/$lib.a
	# echo "[USER] Building $prog"
	make lib DIR=src/lib/$prog LIB=$LIB
done


for prog in $PROGS; do
	BIN=bin/$prog
	# echo "[USER] Building $prog"
	make prog DIR=src/bin/$prog BIN=$BIN
done
