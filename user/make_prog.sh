#!/bin/bash

if [ -z "$1" ];
then
		echo "usage: make_prog.sh <name>"
		exit 1
fi



mkdir -p bin/$1
echo "out = bin/$1" >> bin/$1/Makefile
echo "srcs += main.cpp" >> bin/$1/Makefile

touch bin/$1/main.cpp
