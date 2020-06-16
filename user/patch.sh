#!/bin/bash


patch () {
	truncate --size 0 $1/CMakeLists.txt
	echo 'file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)' >> $1/CMakeLists.txt
	echo "chariot_bin($(basename $1))" >> $1/CMakeLists.txt
}

for i in bin/*
do
	patch ${i}
done
# chariot_bin(init);
