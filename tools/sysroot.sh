#!/bin/bash

ARCH=${ARCH:=x86_64}

mkdir -p build/$ARCH/root

# rsync the root filesystem together
sudo rsync -a base/. build/$ARCH/root/
sudo rsync -a include/ build/$ARCH/root/include
