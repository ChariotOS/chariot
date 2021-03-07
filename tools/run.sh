#!/bin/bash


# make


echo '======================================================================'
tools/sync.sh || exit 1
tools/initrd.sh || exit 1

source .config

# this parses the arch of the elf into "X86-64", "RISC-V", etc...
ARCH=$(readelf -h build/chariot.elf | grep 'Machine' | awk '{print $(NF)}')
QEMU_ARCH="${CONFIG_ARCH_NAME}"
# QEMU_FLAGS="-nographic -serial mon:stdio "
QEMU_FLAGS="-serial mon:stdio "


case $ARCH in 
	X86-64)

		# QEMU_FLAGS+="-enable-kvm -cpu host "
		QEMU_FLAGS+="-m 4G -smp 1 "
		QEMU_FLAGS+="-hda build/chariot.img "
		# QEMU_FLAGS+="-netdev user,id=u1  -device e1000,netdev=u1 "
		# QEMU_FLAGS+="-rtc base=localtime "
		

		# Playing around with USB
		# QEMU_FLAGS+="-device nec-usb-xhci,id=xhci "
		# QEMU_FLAGS+="-device usb-bot "
		QEMU_FLAGS+="-display sdl "

		;;

	RISC-V)


		# QEMU_FLAGS+="-cpu rv64,x-h=true "
		QEMU_FLAGS+="-machine virt -smp 1 -m ${CONFIG_RISCV_RAM_MB}M "
		# QEMU_FLAGS+="-bios none "
		QEMU_FLAGS+="-bios default "
		QEMU_FLAGS+="-kernel build/chariot_initrd.elf "

		QEMU_FLAGS+="-drive file=build/chariot.img,if=none,format=raw,id=x0 "
		QEMU_FLAGS+="-device virtio-blk-device,drive=x0 "
		QEMU_FLAGS+="-device virtio-gpu-device,stats=true "
		QEMU_FLAGS+="-display sdl "

		# virtio keyboard, gpu, and tablet (cursor)
		# QEMU_FLAGS+="-device virtio-tablet-device -device virtio-keyboard-device -device virtio-gpu-device "
		;;


	AArch64)
		QEMU_FLAGS+="-M virt -cpu cortex-a53 -m 1024M "
		QEMU_FLAGS+="-kernel build/chariot.elf "
		;;

esac

echo '======================================================================'

qemu-system-${QEMU_ARCH} ${QEMU_FLAGS} $@
