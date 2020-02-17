#!/usr/bin/env bash

# This is a little bit of a hack, but it is so that libraries
# are built first, then binaries. This fixes `symbol not found' errors

build () {
	echo "Building $1"
	make run M=$1 -j
}


build lib/libc


build src/init
build src/env
build src/cat
build src/echo
build src/sh
build src/ls
build src/test_exit
