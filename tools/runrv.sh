#!/bin/bash


make || exit 1

QEMU_FLAGS="-machine virt -cpu rv64 -smp 4 -m 128M "
QEMU_FLAGS+="-kernel build/root/bin/chariot.elf "
# No graphics and output serial to the tty :^)
QEMU_FLAGS+="-nographic -serial mon:stdio -bios none "
# virtio keyboard, gpu, and tablet (cursor)
QEMU_FLAGS+="-device virtio-tablet-device -device virtio-keyboard-device -device virtio-gpu-device "

# QEMU_FLAGS+="-s -S "


qemu-system-riscv64 ${QEMU_FLAGS} $@
