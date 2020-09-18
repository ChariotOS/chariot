#!/bin/bash

# PATH=


ROOT=$(pwd)/..
TOOLCHAIN=$ROOT/toolchain/local/bin
PATH=$TOOLCHAIN:$PATH


# pushd ..
# 	mkdir -p build
# 	pushd build
# 		cmake libc -GNinja ../
# 		ninja libc
# 		install -D libc/libc.a build/base/lib/libc.a
# 	popd
# popd

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




# llvm libc++

# if [ ! -d llvm-project ]; then
# 	git clone https://github.com/llvm/llvm-project.git llvm-project --depth 2 || (cd llvm-project ; git pull)
# fi # todo: move this to the end of this block:
# 
# 	pushd llvm-project
# 		mkdir -p build
# 		pushd build
# 		 cmake -DCMAKE_C_COMPILER=$TOOLCHAIN/x86_64-elf-chariot-clang       \
# 					 -DCMAKE_CXX_COMPILER=$TOOLCHAIN/x86_64-elf-chariot-clang++     \
# 					 -DLLVM_ENABLE_PROJECTS="libcxx;libcxxabi"                  \
# 					 --sysroot ${ROOT}/build/base/                              \
# 					 ../llvm
# 		popd
# 	popd
# 


popd # src
