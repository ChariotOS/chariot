#!/bin/bash

# sync the root filesystem into build/chariot.img

die() {
    echo "die: $*"
    exit 1
}



echo "Getting sudo so we can mount the disk later on."
sudo id || die "Couldn't get sudo"
echo "Okay!"


IMG=build/chariot.img
mnt=build/mnt
DISK_SIZE_MB=128

mkdir -p build

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


printf "Creating Loopback Device..."
dev=$(sudo losetup --find --partscan --show $IMG)
printf "OK\n"
echo "loopback device is at ${dev}"

cleanup() {

	echo "cleanup"
	if [ -d $mnt ]; then
			printf "unmounting filesystem... "
			sudo umount -f $mnt || ( sleep 1 && sync && sudo umount $mnt )
			rm -rf $mnt
			echo "done"
	fi

	if [ -e "${dev}" ]; then
			printf "cleaning up loopback device... "
			sudo losetup -d "${dev}"
			echo "done"
	fi
}
# clean up when we can
trap cleanup EXIT


# only if the disk wasn't created, create the partition map on the new disk
if [ $disk_exists -eq '0' ]; then
	printf "creating partition table... "
	sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 32k 100% -a minimal set 1 boot on || die "couldn't partition disk"
	echo "done"

	printf "creating new filesystem... "
	sudo mkfs.ext2 "${dev}"p1 || die "couldn't create filesystem"
	echo "done"
fi


printf "mounting... "
# create the mount dir
if [ -d $mnt ]; then
	printf "removing old... "
	sudo umount -f $mnt
	rm -rf $mnt
fi
mkdir -p $mnt
echo "mounted."


sudo mount ${dev}p1 $mnt/

echo 'removing old filesystem data'

# delete all the directories besides boot/
echo 'clearing out old filesystem'

for dir in $mnt/*; do
    [ "$dir" = "$mnt/boot" ] && continue
		echo "removing $dir"
		sudo rm -rf "$dir"
done


echo 'copying new filesystem data...'
sudo cp -a base/. $mnt/
sudo mkdir -p $mnt/dev
sudo mkdir -p $mnt/tmp


# make the user programs
cd user && ./build.sh 2>/dev/null
cd ..

sudo cp -r user/out/bin $mnt/bin
sudo cp -r user/out/lib $mnt/lib
sudo cp -r docs $mnt/etc/


sudo mkdir $mnt/usr/src
sudo rsync -ar --exclude=build  --exclude=.git --exclude=user/out ./ $mnt/usr/src


sudo chown -R 0:0 $mnt

# install the bootloader (grub, in this case)
sudo mkdir -p $mnt/boot/grub
sudo cp grub.cfg $mnt/boot/grub/

# build the kernel and copy it into the boot dir
make -j ARCH=x86_64 || die 'Failed to build the kernel'
sudo nm -s build/vmchariot | c++filt > build/kernel.syms

sudo cp build/vmchariot $mnt/boot/
sudo cp build/kernel.syms $mnt/boot/

# create some device nodes
sudo mknod $mnt/dev/urandom c 1 2
# create the 'console' device
sudo mknod $mnt/dev/console c 20 0
sudo mknod $mnt/dev/mouse c 10 0
sudo mknod $mnt/dev/fb c 21 0
sudo mknod $mnt/dev/sb16 c 22 0

# only install grub on a new disk
if [ $disk_exists -eq '0' ]; then
	sudo grub-install --boot-directory=$mnt/boot --target=i386-pc --modules="ext2 part_msdos" "$dev"
fi

