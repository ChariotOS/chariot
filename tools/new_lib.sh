#!/bin/bash


if [ -z "$1" ];
then
		echo "usage: new_lib.sh <name>"
		exit 1
fi



mkdir -p usr.lib/$1

touch usr.lib/$1/CMakeLists.txt
echo 'file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c *.asm)' >> usr.lib/$1/CMakeLists.txt
echo "chariot_lib(lib$(basename $1) $(basename $1))" >> usr.lib/$1/CMakeLists.txt

touch usr.lib/$1/main.cpp
