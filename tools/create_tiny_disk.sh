#!/bin/bash


die() {
    echo "die: $*"
    exit 1
}

# echo "Getting sudo so we can mount the disk later on."
sudo id > /dev/null || die "Couldn't get sudo"

mkdir -p build

IMG=build/chariot_tiny.img
mnt=build/mnt
DISK_SIZE_MB=16


disk_exists=0

sudo rm -rf "$IMG"


# create the disk image file
dd if=/dev/zero of=$IMG bs=1M count=$DISK_SIZE_MB || die "can't create disk image"
# chown 1000:1000 $IMG || die "couldn't adjust permissions on disk image"
# set the permissions of the disk image
chmod 666 $IMG

dev=$(sudo losetup --find --partscan --show $IMG)

cleanup() {

	# echo "cleanup"
	if [ -d $mnt ]; then
			# printf "unmounting filesystem... "
			sudo umount -f $mnt || ( sleep 1 && sync && sudo umount $mnt )
			# rm -rf $mnt
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

sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 32k 100% -a minimal set 1 boot on || die "couldn't partition disk"
sudo mkfs.ext2 -b 4096 "${dev}"p1 || die "couldn't create filesystem"

# create the mount dir
if [ -d $mnt ]; then
	# printf "removing old... "
	sudo umount -f $mnt
	sudo rm -rf $mnt
fi
mkdir -p $mnt

sudo mount ${dev}p1 $mnt/


make --no-print-directory -j ARCH=x86_64 || die 'Failed to build the kernel'

# setup grub
sudo mkdir -p $mnt/boot/grub
sudo cp meta/grub_embed.cfg $mnt/boot/grub/grub.cfg
sudo cp build/kernel/chariot.elf $mnt/boot/chariot.elf

df $mnt/

sudo grub-install --boot-directory=$mnt/boot --target=i386-pc --modules="ext2 part_msdos" "$dev"
