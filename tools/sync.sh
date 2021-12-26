#!/bin/bash

# sync the root filesystem into build/chariot.img

die() {
    echo "die: $*"
    exit 1
}



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



USING_LOOPBACK="no"

if test -z "$dev"; then
	USING_LOOPBACK="yes"
	if [ $disk_exists -eq '0' ]; then
		if [ "$(uname)" = "Darwin" ]; then
				rm -rf chariot.dmg
				hdiutil create -size ${CONFIG_DISK_SIZE_MB}m -volname Chariot $IMG
			else
				# create the disk image file of size DISK_SIZE_MB
				dd if=/dev/zero of=$IMG bs=1048576 count=$CONFIG_DISK_SIZE_MB || die "can't create disk image"
				chmod 666 $IMG
		fi
	fi
	if [ "$(uname)" = "Darwin" ]; then
		dev=$(hdiutil attach -nomount $IMG | head -n 1 | awk '{print $1}')
	else
		dev=$(sudo losetup --find --partscan --show $IMG)
	fi
	
else
	disk_exists=1
fi


${DIR}/sync_to.sh -p $dev


if [ "${USING_LOOPBACK}" == "yes" ]; then
	if [ "$(uname)" = "Darwin" ]; then
		hdiutil detach "${dev}"
	else
		sudo losetup -d "${dev}"
	fi
fi

