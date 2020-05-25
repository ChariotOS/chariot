#!/bin/bash


# build the root disk and exit if it fails
#     (this is so build failures don't run
#          the kernel and clear the screen)
tools/sync.sh || exit 1

qemu-system-x86_64 -gdb tcp::8256        \
	-nographic -serial mon:stdio           \
	-m 2G -smp 2                           \
	-hda build/chariot.img                 \
	-netdev user,id=u1                     \
	-device e1000,netdev=u1                \
	-rtc base=localtime                    \
	$@


	# -machine kernel_irqchip=on,accel=kvm   \


