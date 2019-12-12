#!/bin/bash

./sync.sh

qemu-system-x86_64 \
	-serial stdio \
	-m 4G -smp 4 \
	-hda build/root.img \
	-device rtl8139
