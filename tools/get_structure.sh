#!/bin/bash

# This file is a terible way to do this... It basically just prints out
# all the directories that the kernel should look in for source files

ARCH=$1

printf "%s " `find kernel -type d`
printf "%s " `find drivers -type d`

printf "%s " `find -L arch/$ARCH -type d`
