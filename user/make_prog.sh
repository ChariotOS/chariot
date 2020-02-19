#!/bin/bash

if [ -z "$1" ];
then
		echo "usage: make_prog.sh <name>"
		exit 1
fi





mkdir -p src/$1
echo "out = bin/$1" >> src/$1/Makefile
echo "srcs += $1.c" >> src/$1/Makefile


touch src/$1/$1.c
