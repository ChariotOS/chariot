#!/bin/bash

# sync the root filesystem into build/chariot.img

die() {
    echo "die: $*"
    exit 1
}

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source $DIR/../.config

# echo "Getting sudo so we can mount the disk later on."
sudo id > /dev/null || die "Couldn't get sudo"
# echo "Okay!"


# x86_64 by default
BUILD=build/
IMG=$BUILD/chariot.img
mnt=$BUILD/mnt

DISK_SIZE_MB=$CONFIG_DISK_SIZE_MB

mkdir -p $BUILD

disk_exists=0


if [ -f "$IMG" ]; then
	disk_exists=1
fi


if [ $disk_exists -eq '0' ]; then
	# create the disk image file
	dd if=/dev/zero of=$IMG bs=1M count=$DISK_SIZE_MB || die "can't create disk image"
	# chown 1000:1000 $IMG || die "couldn't adjust permissions on disk image"
	# set the permissions of the disk image
	chmod 666 $IMG
fi


# printf "Creating Loopback Device..."
dev=$(sudo losetup --find --partscan --show $IMG)
# printf "OK\n"
# echo "loopback device is at ${dev}"

cleanup() {

	# echo "cleanup"
	if [ -d $mnt ]; then
			# printf "unmounting filesystem... "
			sudo umount -f $mnt || ( sleep 1 && sync && sudo umount $mnt )
			rm -rf $mnt
			# echo "done"
	fi

	if [ -e "${dev}" ]; then
			# printf "cleaning up loopback device... "
			sudo losetup -d "${dev}"
			# echo "done"
	fi
}
# clean up when we can
trap cleanup EXIT


# only if the disk wasn't created, create the partition map on the new disk
if [ $disk_exists -eq '0' ]; then
	# printf "creating partition table... "
	sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 32k 100% -a minimal set 1 boot on || die "couldn't partition disk"
	# echo "done"

	# printf "creating new filesystem... "
	sudo mkfs.ext2 -b 4096 "${dev}"p1 || die "couldn't create filesystem"
	# echo "done"
fi


# printf "mounting... "
# create the mount dir
if [ -d $mnt ]; then
	# printf "removing old... "
	sudo umount -f $mnt
	rm -rf $mnt
fi
mkdir -p $mnt
# echo "mounted."


sudo mount ${dev}p1 $mnt/

# echo 'removing old filesystem data'

# delete all the directories besides boot/
# echo 'clearing out old filesystem'


for dir in $mnt/*; do
    [ "$dir" = "$mnt/boot" ] && continue
		# echo "removing $dir"
		# sudo rm -rf "$dir"
done



# create the build/root sysroot directory
tools/sysroot.sh

# build the kernel and copy it into the boot dir
make --no-print-directory -j || die 'Failed to build the kernel'


echo 'Copying filesystem data into the mounted image'

sudo rsync -a $BUILD/root/. $mnt/

sudo mkdir -p $mnt/dev
sudo mkdir -p $mnt/tmp
sudo chown -R 0:0 $mnt

# install the bootloader (grub, in this case)
sudo mkdir -p $mnt/boot/grub
sudo cp kernel/grub.cfg $mnt/boot/grub/

sudo cp $BUILD/root/bin/chariot.elf $mnt/boot/chariot.elf

# only install grub on a new disk
if [ $disk_exists -eq '0' ]; then
	sudo grub-install --boot-directory=$mnt/boot --target=i386-pc --modules="ext2 part_msdos" "$dev"
fi

