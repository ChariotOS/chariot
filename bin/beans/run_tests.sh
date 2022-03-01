#!/bin/bash

make -j

for f in testcode/*.class; do

	echo $f

	bin/hawkbeans-clang-debug $f

done
