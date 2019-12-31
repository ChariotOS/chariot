#!/bin/bash

./sync.sh

qemu-system-x86_64 \
	-nographic -serial mon:stdio \
	-m 4G -smp 4 \
	-hda build/root.img \
	-device rtl8139
