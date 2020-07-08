#!/bin/bash


if [ -z "$1" ];
then
		echo "usage: make_prog.sh <name>"
		exit 1
fi



mkdir -p usr.bin/$1

touch usr.bin/$1/CMakeLists.txt
echo 'file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)' >> usr.bin/$1/CMakeLists.txt
echo "chariot_bin($(basename $1))" >> usr.bin/$1/CMakeLists.txt

touch usr.bin/$1/main.cpp
