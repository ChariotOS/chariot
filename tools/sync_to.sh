#!/bin/bash

# sync the root filesystem into argv0

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



newdisk=0
suffix="p1"
while getopts "ps:" o; do
	case "${o}" in
		# partition the disk before mounting.
		p)
			newdisk=1
			;;
		s)
			suffix="${OPTARG}"
			;;
	esac
done
shift $((OPTIND-1))

dev=$1

if [ "$dev" == "" ]; then
	echo "usage: tools/sync_to.sh <block device>"
	exit
fi

fsdev=$(sudo fdisk -l "${dev}" | grep -A10 "Device" | sed 1,1d | awk '{print $1}')

if [ "$fsdev" == "" ]; then
	echo "Running parted..."
	# sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 32k 100% -a minimal set 1 boot on || die "couldn't partition disk"
	sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 32k 100% -a minimal set 1 boot on || die "couldn't partition disk"
	fsdev=$(sudo fdisk -l "${dev}" | grep -A10 "Device" | sed 1,1d | awk '{print $1}')

	echo "Making filesystem"

	# sudo mke2fs -L "Chariot Root" -v -b 4096 -q -I 128 "${fsdev}" || die "couldn't create filesystem"
	sudo mkfs.ext2 -L "Chariot Root" -v -b 4096 -q -I 128 "${fsdev}" || die "couldn't create filesystem"
	echo "created."
	newdisk=1
fi


echo "Device: ${dev}, losetup: ${USING_LOOPBACK}, ${disk_exists}"


cleanup() {
	echo "Cleanup"
	if [ -d $mnt ]; then
		sudo sync -f $mnt
		sudo umount -f $mnt || ( sleep 1 && sync && sudo umount $mnt )
		rm -rf $mnt
	fi
	sudo sync $dev
}
# clean up when we can
trap cleanup EXIT

# create the mount dir
if [ -d $mnt ]; then
	sudo umount -f $mnt | die "failed to unmount old disk"
	rm -rf $mnt
fi
mkdir -p $mnt


sudo mount ${fsdev} $mnt/

# create the build/root sysroot directory

if [ -n "$CONFIG_ENABLE_USERSPACE" ]; then
	${DIR}/sysroot.sh
fi


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
	if [ "$newdisk" == '1' ]; then
		sudo grub-install --boot-directory=$mnt/boot --target=i386-pc --modules="ext2 part_msdos" "$dev"
	fi
fi

USED=$(du -s -BM ${mnt} | awk '{ print $1 }')
echo "Root filesystem usage: $USED"
