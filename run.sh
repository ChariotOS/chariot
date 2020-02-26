#!/bin/bash


# build the root disk and exit if it fails
#     (this is so build failures don't run
#          the kernel and clear the screen)
./sync.sh || exit 1

qemu-system-x86_64 -gdb tcp::8256            \
	-nographic -serial mon:stdio               \
	-m 256M -smp 4                               \
	-hda build/chariot.img                     \
	$@


# -machine pc-q35-2.8                        \
