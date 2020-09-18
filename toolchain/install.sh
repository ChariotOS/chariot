#!/bin/bash


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

mkdir -p $DIR/tarballs
mkdir -p $DIR/local

ARCH="x86_64"
TARGET="$ARCH-elf-chariot"
PREFIX="$DIR/local"
SYSROOT="$DIR/../build/base/"



BINUTILS_VERSION="2.33.1"
GCC_VERSION="10.1.0"

pushd tarballs


	if [ ! -f binutils.tar ]; then
		wget ftp://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz -O binutils.tar
	fi


	if [ ! -d binutils-${BINUTILS_VERSION} ]; then
		echo "Unpacking binutils..."
		tar -xf binutils.tar

		pushd binutils-${BINUTILS_VERSION}
			echo "Initializing Git Repo..."
			git init >/dev/null
			git add . >/dev/null
			git commit -am "BASE" >/dev/null

			echo "Applying Patch..."
			git apply ../../binutils.patch >/dev/null

		popd
	fi


#	# GCC
if [ ! -f gcc-${GCC_VERSION}.tar ]; then
	wget ftp://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz -O gcc-${GCC_VERSION}.tar
fi

if [ ! -d gcc-${GCC_VERSION} ]; then
	echo "Unpacking gcc..."
	tar -xf gcc-${GCC_VERSION}.tar

	pushd gcc-${GCC_VERSION}
		echo "Initializing Git Repo..."

		echo "Applying Patch..."
		patch -p1 < ../../gcc.patch
		# git apply ../../gcc.patch >/dev/null
	popd
fi

popd



mkdir -p $DIR/build
mkdir -p $DIR/build/gcc
mkdir -p $DIR/build/binutils

MAKEJOBS=24



# mkdir -p $PREFIX/bin
# pushd $PREFIX/bin
# 	for prog in clang clang++; do
# 		echo "#!/bin/bash" > $TARGET-$prog
# 		echo "ARCH=$ARCH CROSS_COMPILE=$TARGET $prog --sysroot $SYSROOT \$@" >> $TARGET-$prog
# 		echo +x $TARGET-$prog
# 	done
# popd


pushd build
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
		"$DIR"/tarballs/gcc-${GCC_VERSION}/configure --prefix="$PREFIX" \
																			 --target="$TARGET" \
																			 --with-sysroot="$SYSROOT" \
																			 --disable-nls \
																			 --with-newlib \
																			 --enable-shared \
																			 --enable-languages=c,c++ || exit 1

		echo "XXX build gcc and libgcc"
		make -j "$MAKEJOBS" all-gcc all-target-libgcc || exit 1
		echo "XXX install gcc and libgcc"
		make install-gcc install-target-libgcc || exit 1


		echo "XXX build libstdc++"
		make all-target-libstdc++-v3
		# echo "XXX install libstdc++"
		# make install-target-libstdc++-v3 || exit 1
	popd
popd


