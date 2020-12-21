#!/bin/bash

mkdir -p build/root

# rsync the root filesystem together
sudo rsync -a base/. build/root/
sudo rsync -a include/ build/root/include
