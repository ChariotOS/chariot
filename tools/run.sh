#!/bin/bash

make

# this parses the arch of the elf into "X86-64", "RISC-V", etc...
ARCH=$(readelf -h build/root/bin/chariot.elf | grep 'Machine' | awk '{print $(NF)}')
QEMU_ARCH=""
QEMU_FLAGS=""




case $ARCH in 
	X86-64)
		tools/sync.sh || exit 1
		QEMU_ARCH="x86_64"
		QEMU_FLAGS+="-nographic -serial mon:stdio "
		QEMU_FLAGS+="-m 4G -smp 4 "
		QEMU_FLAGS+="-hda build/chariot.img "
		QEMU_FLAGS+="-netdev user,id=u1  -device e1000,netdev=u1 "
		QEMU_FLAGS+="-rtc base=localtime "
		;;

	RISC-V)
		QEMU_ARCH=riscv64
		QEMU_FLAGS+="-machine virt -cpu rv64 -smp 4 -m 128M "
		QEMU_FLAGS+="-kernel build/root/bin/chariot.elf "
		# No graphics and output serial to the tty :^)
		QEMU_FLAGS+="-nographic -serial mon:stdio -bios none "
		# virtio keyboard, gpu, and tablet (cursor)
		QEMU_FLAGS+="-device virtio-tablet-device -device virtio-keyboard-device -device virtio-gpu-device "
		;;
esac


qemu-system-${QEMU_ARCH} ${QEMU_FLAGS}

