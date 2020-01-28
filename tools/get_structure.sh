#!/bin/bash

ARCH=x86

printf "%s " `find kernel -type d`

printf "%s " `find arch/$ARCH -type d`
