#!/bin/bash


if [ -z "$1" ];
then
		echo "usage: make_prog.sh <name>"
		exit 1
fi



mkdir -p bin/$1

touch bin/$1/CMakeLists.txt
echo 'file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)' >> bin/$1/CMakeLists.txt
echo "chariot_bin($(basename $1))" >> bin/$1/CMakeLists.txt

touch bin/$1/main.cpp
