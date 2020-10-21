#!/bin/bash

# PATH=



ROOT=$(pwd)/..
SYSROOT=$ROOT/build/root
TOOLCHAIN=$ROOT/toolchain/local/bin
export PATH=$TOOLCHAIN:$PATH



pushd ../
	tools/sysroot.sh
popd

mkdir -p src

pushd src

if [ ! -d freetype-2.10.1 ]; then
	wget "https://download.savannah.gnu.org/releases/freetype/freetype-2.10.1.tar.gz"
	tar -xzvf freetype-2.10.1.tar.gz
	export BUILD_DIR=freetype-2.10.1
	rm freetype-2.10.1.tar.gz

	pushd freetype-2.10.1
		patch -p1 < ../../patches/freetype.patch
	export CC=x86_64-elf-chariot-gcc

	./configure                      \
				--host=x86_64-elf-chariot  \
				--with-harfbuzz=no         \
				--with-bzip2=no            \
				--with-zlib=no             \
				--with-png=no              \
				--disable-shared \
				"CFLAGS=-I$ROOT/usr.include -fno-stack-protector -DUSERLAND" \
				"CXXFLAGS=-I$ROOT/usr.include -fno-stack-protector -DUSERLAND"
	make -j
	popd
fi

exit






if [ ! -d mesa ]; then
# mesa
git clone https://github.com/nickwanninger/mesa-chariot.git mesa --depth 1 || (cd mesa ; git pull)
	pushd mesa

		./configure                                             \
					--prefix=${MESA_BIN}                              \
					--with-osmesa-bits=32                             \
					--with-driver=osmesa                              \
					--disable-shared                                  \
					--enable-static                                   \
					--without-x                                       \
					--disable-egl                                     \
					--disable-glw                                     \
					--disable-glut                                    \
					--disable-driglx-direct                           \
					--disable-gallium                                 \
					"CFLAGS=-I$ROOT/usr.include -fno-stack-protector" \
					"CXXFLAGS=-I$ROOT/usr.include -fno-stack-protector"

		make -j --no-print-directory
		echo "XXX Built Mesa!"

	popd
fi







popd # src
