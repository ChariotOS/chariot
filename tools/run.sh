#!/bin/bash


# make


echo '======================================================================'
tools/sync.sh || exit 1
# tools/initrd.sh || exit 1

source .config

# this parses the arch of the elf into "X86-64", "RISC-V", etc...
ARCH=$(readelf -h build/chariot.elf | grep 'Machine' | awk '{print $(NF)}')
QEMU_ARCH="${CONFIG_ARCH_NAME}"
# QEMU_FLAGS="-nographic -serial mon:stdio "
QEMU_FLAGS="-serial mon:stdio "

# -chardev stdio,id=stdout,mux=on
# -device virtconsole,chardev=stdout
# -device isa-debugcon,chardev=stdout

#  QEMU_FLAGS+="-d int -D ints.out "

case $ARCH in 
	X86-64)
		QEMU_FLAGS+="-enable-kvm -cpu host "
		QEMU_FLAGS+="-global kvm-pit.lost_tick_policy=discard "
		QEMU_FLAGS+="-m 4G "
		QEMU_FLAGS+="-smp ${CONFIG_QEMU_CORES} "
		QEMU_FLAGS+="-hda build/chariot.img "

		QEMU_FLAGS+="-device VGA,vgamem_mb=64 "
		QEMU_FLAGS+="-netdev user,id=u1  -device e1000,netdev=u1 "
		QEMU_FLAGS+="-rtc base=localtime "
		# QEMU_FLAGS+="-device sb16 "
		QEMU_FLAGS+="-display sdl "
		;;

	RISC-V)
		QEMU_FLAGS+="-smp 4 "
		QEMU_FLAGS+="-bios default "
		QEMU_FLAGS+="-machine sifive_u "
		QEMU_FLAGS+="-kernel build/chariot.elf "
		;;

	RISC-V-virt)
		QEMU_FLAGS+="-machine virt -smp 4 -m ${CONFIG_RISCV_RAM_MB}M "
		QEMU_FLAGS+="-bios default "
		QEMU_FLAGS+="-kernel build/chariot.elf "

		QEMU_FLAGS+="-drive file=build/chariot.img,if=none,format=raw,id=x0 "
		QEMU_FLAGS+="-device virtio-blk-device,drive=x0 "
		QEMU_FLAGS+="-device virtio-gpu-device,xres=${CONFIG_FRAMEBUFFER_WIDTH},yres=${CONFIG_FRAMEBUFFER_HEIGHT} "

		QEMU_FLAGS+="-device virtio-keyboard-device "
		QEMU_FLAGS+="-device virtio-mouse-device "
		# QEMU_FLAGS+="-display sdl "
		;;


	AArch64)
		# QEMU_FLAGS+="-M virt -smp 1 -cpu cortex-a53 -m 1024M "
		QEMU_FLAGS+="-accel tcg "
		QEMU_FLAGS+="-M type=raspi3 -m 1024 "
		QEMU_FLAGS+="-kernel build/chariot.elf "
		;;

esac

echo '======================================================================'

qemu-system-${QEMU_ARCH} ${QEMU_FLAGS} $@
