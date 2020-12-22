#!/bin/bash


make || exit 1

qemu-system-aarch64                      \
	-nographic -M raspi3                   \
	-kernel build/root/bin/chariot.elf

