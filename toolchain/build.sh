#!/bin/bash


if [ $# -eq 0 ]; then
	echo "Usage build.sh [arch...]"
	exit -1
fi


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export PATH="/usr/local/bin:/usr/bin:/bin"



source $DIR/../.config


ROOT=$DIR/..
SYSROOT="$ROOT/build/root/"

BINUTILS_VERSION="2.33.1"
GCC_VERSION="$CONFIG_GCC_VERSION"


mkdir -p $DIR/src
mkdir -p $DIR/local


MESSAGE="echo"
if which figlet >/dev/null 2>&1; then
	MESSAGE="figlet"
fi



# Download all the sources we need
pushd src
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

  if [ "$(uname)" = "Darwin" ]; then
  	pushd "gcc-${GCC_VERSION}"
    	./contrib/download_prerequisites
    popd
  fi
popd



if [ "$(uname)" = "Darwin" ]; then
 	# export CC=clang
	# export CXX=clang++
	echo "MACOS"
fi


for ARCH in "$@"
do

	TARGET="$ARCH-elf"
	PREFIX="$DIR/local"



	BUILD=$DIR/build/$ARCH
	mkdir -p $BUILD
	mkdir -p $BUILD/gcc
	mkdir -p $BUILD/binutils
	MAKEJOBS=$(nproc)


	pushd $BUILD

		# build binutils
		pushd binutils

			$MESSAGE "BINUTILS CONFIGURE"

			"$DIR"/src/binutils-${BINUTILS_VERSION}/configure --prefix="$PREFIX" \
																							--target="$TARGET" \
																							--with-sysroot="$SYSROOT" \
																							--enable-shared \
																							--disable-nls || exit 1
			$MESSAGE "BINUTILS MAKE"
			make -j "$MAKEJOBS" || exit 1
			make install || exit 1
		popd

		$MESSAGE "BINUTILS DONE"


		# build gcc
		pushd gcc

			$MESSAGE "GCC CONFIGURE"

			"$DIR"/src/gcc-${GCC_VERSION}/configure --prefix="$PREFIX" \
																				 --target="$TARGET" \
																				 --with-sysroot="$SYSROOT" \
																				 --disable-bootstrap \
																				 --disable-nls \
																				 --with-newlib \
																				 --enable-shared \
																				 --enable-languages=c,c++ || exit 1

			# we gotta do this casue libstdc++-v3 wants to check our header files out
			mkdir -p $SYSROOT
			$MESSAGE "GCC CONFIGURE DONE"

			if [ "$(uname)" = "Darwin" ]; then
      	# under macOS generated makefiles are not resolving the "intl"
        # dependency properly to allow linking its own copy of
        # libintl when building with --enable-shared.
        make -j "$MAKEJOBS" || true
        pushd intl
        	make all-yes
        popd
      fi

			$MESSAGE "GCC MAKE"
			echo "XXX build gcc and libgcc"
			make -j "$MAKEJOBS" all-gcc all-target-libgcc || exit 1
			echo "XXX install gcc and libgcc"
			make install-gcc install-target-libgcc || exit 1
		popd

		
	popd

done
