#!/usr/bin/env bash

PROGS=$(ls src)


mkdir -p bin



for prog in $PROGS; do
	BIN=bin/$prog
	# echo "[USER] Building $prog"
	make $BIN DIR=src/$prog BIN=$BIN
done
