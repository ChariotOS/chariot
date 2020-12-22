#!/bin/bash


if [ $# -eq 0 ]; then
	echo "Usage build.sh [arch...]"
	exit -1
fi


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

mkdir -p $DIR/tarballs
mkdir -p $DIR/local

# Download all the sources we need
pushd tarballs
	if [ ! -f binutils.tar ]; then
		wget ftp://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz -O binutils.tar
	fi

	if [ ! -d binutils-${BINUTILS_VERSION} ]; then
		echo "Unpacking binutils..."
		tar -xf binutils.tar
	fi

	#	# GCC
	if [ ! -f gcc-${GCC_VERSION}.tar ]; then
		wget ftp://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz -O gcc-${GCC_VERSION}.tar
	fi

	if [ ! -d gcc-${GCC_VERSION} ]; then
		echo "Unpacking gcc..."
		tar -xf gcc-${GCC_VERSION}.tar
	fi
popd



for ARCH in "$@"
do

	TARGET="$ARCH-elf"
	PREFIX="$DIR/local"

	ROOT=$DIR/..
	SYSROOT="$ROOT/build/root/"

	BINUTILS_VERSION="2.33.1"
	GCC_VERSION="10.1.0"

	BUILD=$DIR/build/$ARCH
	mkdir -p $BUILD
	mkdir -p $BUILD/gcc
	mkdir -p $BUILD/binutils
	MAKEJOBS=24


	pushd $BUILD
		# build binutils
		pushd binutils
			rm -f ./config.cache # Let's do this in case someone has already built the i686 version
			"$DIR"/tarballs/binutils-${BINUTILS_VERSION}/configure --prefix="$PREFIX" \
																							--target="$TARGET" \
																							--with-sysroot="$SYSROOT" \
																							--enable-shared \
																							--disable-nls || exit 1
			make -j "$MAKEJOBS" || exit 1
			make install || exit 1
		popd


		# build gcc
		pushd gcc
			rm -f ./config.cache # Let's do this in case someone has already built the i686 version
			"$DIR"/tarballs/gcc-${GCC_VERSION}/configure --prefix="$PREFIX" \
																				 --target="$TARGET" \
																				 --with-sysroot="$SYSROOT" \
																				 --disable-nls \
																				 --with-newlib \
																				 --enable-shared \
																				 --enable-languages=c,c++ || exit 1

			# we gotta do this casue libstdc++-v3 wants to check our header files out
			mkdir -p $SYSROOT

			echo "XXX build gcc and libgcc"
			make -j "$MAKEJOBS" all-gcc all-target-libgcc || exit 1
			echo "XXX install gcc and libgcc"
			make install-gcc install-target-libgcc || exit 1
		popd
	popd

done
