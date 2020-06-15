#!/usr/bin/env bash

# This is a little bit of a hack, but it is so that libraries
# are built first, then binaries. This fixes `symbol not found' errors


PFX=

if command -v bear; then
	PFX=bear
fi

build () {
	eval $PFX make run M=$1 --no-print-directory
}

# bit of a hack
eval $PFX make ../build/userspace/crt0.o

build lib/libc

# build everything in parallel then wait on it
for i in bin/*
do
	build ${i}
done

# wait

# make -C rs
# cp $(find rs/target/chariot-x86_64/ -maxdepth 2 -executable -type f) out/bin/
