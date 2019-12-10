#!/bin/bash

./sync.sh

qemu-system-x86_64 -m 2G -smp 4 -hda build/root.img -nographic
