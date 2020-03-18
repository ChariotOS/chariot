#!/bin/bash

IMG=build/chariot.img
dev=$(sudo losetup --find --partscan --show $IMG)

fsck -n ${dev}p1

sudo losetup -d "${dev}"
