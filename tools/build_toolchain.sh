#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source $DIR/../.config


if [ ! -f toolchain/local/bin/${CONFIG_ARCH_NAME}-elf-gcc ]; then
	echo "-- Toolchain (GCC) for ${CONFIG_ARCH_NAME} does not exist. Building in 3 seconds..."
	sleep 3
	pushd toolchain
		./build.sh "${CONFIG_ARCH_NAME}"
	popd
fi


if [ ! -f ports/out/${CONFIG_ARCH_NAME}/lib/libfreetype.a ]; then
	echo "-- Ports for ${CONFIG_ARCH_NAME} do not exist. Building in 3 seconds..."
	sleep 3
	pushd ports
		./build.sh "${CONFIG_ARCH_NAME}"
	popd
fi
