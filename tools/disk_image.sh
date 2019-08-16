#!/bin/bash

set -e

die() {
    echo "die: $@"
    exit 1
}

if [ $(id -u) != 0 ]; then
    die "this script needs to run as root"
fi

grub=$(which grub-install 2>/dev/null) || true
if [[ -z "$grub" ]]; then
    grub=$(which grub2-install 2>/dev/null) || true
fi
if [ -z "$grub" ]; then
    echo "can't find a grub-install or grub2-install binary, oh no"
    exit 1
fi
echo "using grub-install at ${grub}"


diskimage=build/disk.img
mkdir -p build

echo "setting up disk image..."
dd if=/dev/zero of=$diskimage bs=1M count=${DISK_SIZE:-50} status=none || die "couldn't create disk image"
chown 1000:1000 $diskimage || die "couldn't adjust permissions on disk image"
echo "done"

echo -n "creating loopback device... "
dev=$(losetup --find --partscan --show $diskimage)
if [ -z $dev ]; then
    die "couldn't mount loopback device"
fi
echo "loopback device is at ${dev}"


cleanup() {
	if [ -d build/mnt ]; then
			echo -n "unmounting filesystem... "
			umount build/mnt || ( sleep 1 && sync && umount build/mnt )
			rm -rf build/mnt
			echo "done"
	fi

	if [ -e ${dev} ]; then
			echo -n "cleaning up loopback device... "
			losetup -d ${dev}
			echo "done"
	fi
}


trap cleanup EXIT


echo -n "creating partition table... "
parted -s ${dev} mklabel msdos mkpart primary ext2 32k 100% -a minimal set 1 boot on || die "couldn't partition disk"
echo "done"

echo -n "destroying old filesystem... "
dd if=/dev/zero of=${dev}p1 bs=1M count=1 status=none || die "couldn't destroy old filesystem"
echo "done"

echo -n "creating new filesystem... "
mke2fs -q -I 128 ${dev}p1 || die "couldn't create filesystem"
echo "done"

echo -n "mounting filesystem... "
mkdir -p build/mnt
mount ${dev}p1 build/mnt/ || die "couldn't mount filesystem"
echo "done"


echo -n "creating /boot... "
mkdir -p build/mnt/boot
echo "done"

echo "installing grub using $grub..."
$grub --boot-directory=build/mnt/boot --modules="ext2 part_msdos" ${dev}


if [ -d build/mnt/boot/grub2 ]; then
	cp grub.cfg build/mnt/boot/grub2/grub.cfg
else
	cp grub.cfg build/mnt/boot/grub/grub.cfg
fi

echo "done"

echo -n "installing kernel in /boot... "
cp build/kernel.elf build/mnt/boot
echo "done"
