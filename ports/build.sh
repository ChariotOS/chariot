#!/bin/bash

# PATH=

ROOT=$(pwd)/..
PATH=$ROOT/toolchain/local/bin:$PATH

mkdir -p src

pushd src

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

popd
