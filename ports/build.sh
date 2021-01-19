#!/bin/bash


pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd "$@" > /dev/null
}


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $DIR/../.config

ROOT=$(realpath $DIR/..)
SYSROOT=$ROOT/build/root
TOOLCHAIN=$ROOT/toolchain/local/bin
export PATH=$TOOLCHAIN:$PATH

pushd ../
	tools/sysroot.sh
popd


FREETYPE_NAME=freetype-2.10.1

mkdir -p src
pushd src

	# Download freetype if needed
	if [ ! -d $FREETYPE_NAME ]; then
		wget "https://download.savannah.gnu.org/releases/freetype/$FREETYPE_NAME.tar.gz"
		tar -xzvf $FREETYPE_NAME.tar.gz
		rm $FREETYPE_NAME.tar.gz
		pushd $FREETYPE_NAME
			patch -p1 < ../../patches/freetype.patch
		popd
	fi
popd


for ARCH in "$@"
do

	mkdir -p build/$ARCH
	pushd build/$ARCH

		mkdir -p freetype
		pushd freetype
			$ARCH-elf-gcc -v

			$ROOT/ports/src/$FREETYPE_NAME/configure                    \
					--host=$ARCH-elf                                        \
					--prefix=$ROOT/ports/out/$ARCH/                         \
					--with-harfbuzz=no                                      \
					--with-bzip2=no                                         \
					--with-zlib=no                                          \
					--with-png=no                                           \
					--disable-shared                                        \
					"CFLAGS=-I$ROOT/include -fno-stack-protector -DUSERLAND -nostdlib -s " \
					"CXXFLAGS=-I$ROOT/include -fno-stack-protector -DUSERLAND -nostdlib -s "

			
			make -j
			make install

		popd
	popd

done
