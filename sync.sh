#!/bin/bash

# sync the root filesystem into build/root.img

IMG=build/root.img
MOUNTPOINT=build/mnt
DISK_SIZE_MB=30

rm -rf $IMG


# create the disk image file
dd if=/dev/urandom of=$IMG bs=1M count=$DISK_SIZE_MB

# set the permissions of the disk image
chmod 666 $IMG


printf "building filesystem..."
mkfs.ext2 $IMG
printf "OK\n"

printf "Making the mountpoint..."
rm -rf build/mnt
mkdir -p build/mnt
printf "OK\n"


sudo umount $MOUNTPOINT
printf "Mounting the disk, may need sudo...\n"
sudo mount -o loop $IMG $MOUNTPOINT || exit
printf "OK\n"


sudo cp -r mnt/. $MOUNTPOINT/
sudo cp -r src $MOUNTPOINT/src
sudo cp -r include $MOUNTPOINT/include

sudo umount $MOUNTPOINT
