#!/bin/bash

# sync the root filesystem into build/chariot.img

die() {
    echo "die: $*"
    exit 1
}




# build the kernel and whatnot
make --no-print-directory -j || die 'Failed to build the kernel'


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source $DIR/../.config

if [ "$(id -u)" != 0 ]; then
    exec sudo -E -- "$0" "$@" || die "this script needs to run as root"
else
    : "${SUDO_UID:=0}" "${SUDO_GID:=0}"
fi

# x86_64 by default
BUILD=build
IMG=$BUILD/chariot.img
mnt=$BUILD/mnt

DISK_SIZE_MB=$CONFIG_DISK_SIZE_MB

mkdir -p $BUILD


if [ "$(uname)" = "Darwin" ]; then
	IMG=$BUILD/chariot.dmg
else
	IMG=$BUILD/chariot.img
fi


disk_exists=0
if [ -f "$IMG" ]; then
	disk_exists=1
fi



# DEV=$(hdiutil attach -nomount chariot.dmg | head -n 1 | awk '{print $1}')
# echo $DEV
# hdiutil detach $DEV


USING_LOOPBACK="no"

if test -z "$dev"; then
	USING_LOOPBACK="yes"
	if [ $disk_exists -eq '0' ]; then
		if [ "$(uname)" = "Darwin" ]; then
				rm -rf chariot.dmg
				hdiutil create -size ${CONFIG_DISK_SIZE_MB}m -volname Chariot $IMG
			else
				# create the disk image file of size DISK_SIZE_MB
				dd if=/dev/zero of=$IMG bs=1000000 count=$CONFIG_DISK_SIZE_MB || die "can't create disk image"
				chmod 666 $IMG
		fi
	fi
	# mount the disk with loopback or whatever
	if [ "$(uname)" = "Darwin" ]; then
		dev=$(hdiutil attach -nomount $IMG | head -n 1 | awk '{print $1}')
	else
		dev=$(sudo losetup --find --partscan --show $IMG)
	fi
	
else
	disk_exists=1
fi



echo "Device: ${dev}, losetup: ${USING_LOOPBACK}, ${disk_exists}"


# if [ "$CONFIG_X86" != "" ]; then
if [ "$fsdev" == "" ]; then
	fsdev="${dev}p1"
fi
# fi


cleanup() {
	echo "Cleanup"
	if [ -d $mnt ]; then
			sudo umount -f $mnt || ( sleep 1 && sync && sudo umount $mnt )
			rm -rf $mnt
	fi

	if [ "${USING_LOOPBACK}" == "yes" ]; then
		if [ "$(uname)" = "Darwin" ]; then
			hdiutil detach "${dev}"
		else
			sudo losetup -d "${dev}"
		fi
	fi

}
# clean up when we can
trap cleanup EXIT


# only if the disk wasn't created, create the partition map on the new disk
if [ $disk_exists -eq '0' ]; then
	printf "Running parted..."
	sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 32k 100% -a minimal set 1 boot on || die "couldn't partition disk"
	# sudo mkfs.ext2 -b 4096 "${fsdev}" || die "couldn't create filesystem"
	printf "Done.\n"

	printf "Running mke2fs on ${fsdev}..."
	sudo mke2fs -L "Chariot Root" -v -b 4096 -q -I 128 "${fsdev}" || die "couldn't create filesystem"
	printf "Done.\n"

fi

# create the mount dir
if [ -d $mnt ]; then
	sudo umount -f $mnt
	rm -rf $mnt
fi
mkdir -p $mnt


sudo mount ${fsdev} $mnt/

# create the build/root sysroot directory
tools/sysroot.sh





echo 'Copying filesystem data into the mounted image'
sudo rsync -a $BUILD/root/. $mnt/
sudo mkdir -p $mnt/dev
sudo mkdir -p $mnt/tmp
sudo mkdir -p $mnt/proc
sudo chown -R 0:0 $mnt

if [ -n "$CONFIG_X86" ]; then
	# install the bootloader (grub, in this case)
	sudo mkdir -p $mnt/boot/grub
	sudo cp arch/x86/grub.cfg $mnt/boot/grub/
	sudo cp $BUILD/chariot.elf $mnt/boot/chariot.elf
	# only install grub on a new disk
	if [ $disk_exists -eq '0' ]; then
		sudo grub-install --boot-directory=$mnt/boot --target=i386-pc --modules="ext2 part_msdos" "$dev"
	fi
fi



USED=$(du -s -BM ${mnt} | awk '{ print $1 }')
echo "Root filesystem usage: $USED/${CONFIG_DISK_SIZE_MB}M"
