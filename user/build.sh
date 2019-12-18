#!/usr/bin/env bash

# This is a little bit of a hack, but it is so that libraries
# are built first, then binaries. This fixes `symbol not found' errors
for dir in $(ls -d lib/*); do
	echo "Building $dir"
	make run M=$dir
done


for dir in $(ls -d src/*); do
	echo "Building $dir"
	make run M=$dir
done
