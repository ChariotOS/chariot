#!/usr/bin/env bash

# This is a little bit of a hack, but it is so that libraries
# are built first, then binaries. This fixes `symbol not found' errors

build () {
	echo "Building $1"
	make run M=$1 -j
}


build lib/libc
build lib/libm

for i in src/*
do
	build ${i}
done
