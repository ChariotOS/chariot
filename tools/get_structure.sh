#!/bin/bash

ARCH=x86

printf "%s " `find src -type d | grep -v arch`

printf "%s " `find src/arch/$ARCH -type d`
