#!/bin/bash

set -e
set -o pipefail


if [ $# -eq 0 ]; then
	echo "Usage build.sh [arch...]"
	exit -1
fi


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export PATH="/usr/local/bin:/usr/bin:/bin"



source $DIR/../.config

MAKE=make

ROOT=$DIR/..
SYSROOT="$ROOT/build/root/"

BINUTILS_VERSION="2.38"
GCC_VERSION="$CONFIG_GCC_VERSION"


mkdir -p $DIR/src
mkdir -p $DIR/local


MESSAGE="echo"
if which figlet >/dev/null 2>&1; then
	MESSAGE="figlet"
fi

CURRENT="download"

buildstep() {
    NAME=$1
    shift
    "$@" 2>&1 | sed $'s|^|\x1b[34m['"${CURRENT}: ${NAME}"$']\x1b[39m |' || exit 1
}



# Download all the sources we need
pushd src
	if [ ! -f binutils.tar ]; then
		wget ftp://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz -O binutils.tar
	fi

	if [ ! -d binutils-${BINUTILS_VERSION} ]; then
		echo "Unpacking binutils..."
		buildstep "binutils/unpack" tar -xf binutils.tar
	fi

	#	# GCC
	if [ ! -f gcc-${GCC_VERSION}.tar ]; then
		wget ftp://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz -O gcc-${GCC_VERSION}.tar
	fi

	if [ ! -d gcc-${GCC_VERSION} ]; then
		echo "Unpacking gcc..."
		buildstep "gcc/unpack" tar -xf gcc-${GCC_VERSION}.tar
	fi

  if [ "$(uname)" = "Darwin" ]; then
  	pushd "gcc-${GCC_VERSION}"
    	buildstep "gcc/prereq" ./contrib/download_prerequisites
    popd
  fi
popd



if [ "$(uname)" = "Darwin" ]; then
 	# export CC=clang
	# export CXX=clang++
	echo "MACOS"
fi



# we gotta do this casue libstdc++-v3 wants to check our header files out
mkdir -p $SYSROOT
mkdir -p $SYSROOT/usr/include/

echo $ROOT
FILES=$(find "$ROOT/libc/include/" "$ROOT/include/" -name '*.h' -print)
for header in $FILES; do
		target=$(echo "$header" | sed -e "s@$ROOT/libc/include@@"  -e "s@$ROOT/include@@")
		buildstep "system_headers" install -D "$header" "$SYSROOT/usr/include/$target"
done

for ARCH in "$@"
do

	TARGET="$ARCH-elf"
	PREFIX="$DIR/local"

	CURRENT=$ARCH


	BUILD=$DIR/build/$ARCH
	mkdir -p $BUILD
	mkdir -p $BUILD/gcc
	mkdir -p $BUILD/binutils
	MAKEJOBS=$(nproc)


	pushd $BUILD

		# build binutils
		pushd binutils

			buildstep "binutils/configure" "$DIR"/src/binutils-${BINUTILS_VERSION}/configure --prefix="$PREFIX" \
																							--target="$TARGET" \
																							--with-sysroot="$SYSROOT" \
																							--enable-shared \
																							--disable-nls || exit 1
			buildstep "binutils/make"    make -j "$MAKEJOBS" || exit 1
			buildstep "binutils/install" make install || exit 1
		popd

		$MESSAGE "BINUTILS DONE"


		# build gcc
		pushd gcc

			buildstep "gcc/configure" "$DIR"/src/gcc-${GCC_VERSION}/configure --prefix="$PREFIX" \
																				 --target="$TARGET" \
																				 --with-sysroot="$SYSROOT" \
																				 --disable-bootstrap \
																				 --disable-nls \
																				 --with-newlib \
																				 --enable-shared \
																				 --enable-languages=c,c++ || exit 1

			if [ "$(uname)" = "Darwin" ]; then
      	# under macOS generated makefiles are not resolving the "intl"
        # dependency properly to allow linking its own copy of
        # libintl when building with --enable-shared.
        make -j "$MAKEJOBS" || true
        pushd intl
        	make all-yes
        popd
      fi

			buildstep "gcc/build"          make -j "$MAKEJOBS" all-gcc || exit 1
			buildstep "libgcc/build"       make -j "$MAKEJOBS" all-target-libgcc || exit 1
			buildstep "gcc+libgcc/install" make -j "$MAKEJOBS" install-gcc install-target-libgcc || exit 1

			# buildstep "libstdc++/build" "$MAKE" -j "$MAKEJOBS" all-target-libstdc++-v3 || exit 1
			# buildstep "libstdc++/install" "$MAKE" install-target-libstdc++-v3 || exit 1

		popd

		
	popd

done


echo "Done compiling. Stripping output"

strip local/bin/*
