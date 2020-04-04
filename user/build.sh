#!/usr/bin/env bash

# This is a little bit of a hack, but it is so that libraries
# are built first, then binaries. This fixes `symbol not found' errors

build () {
	make run M=$1 -j --no-print-directory
}


build lib/libc

# build lib/liblua

for i in bin/*
do
	build ${i}
done
