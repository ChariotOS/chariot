#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source $DIR/../.config


LIBGCC="$DIR/../toolchain/build/${CONFIG_ARCH_NAME}/gcc/gcc/libgcc.a"

pushd $DIR/../ >/dev/null

# build chariot if it hasn't been
if [ ! -f build/root/lib/libchariot.a ]; then
	make
fi

# reasonable page size :)
LDFLAGS="-z max-page-size=0x1000"

pushd build >/dev/null

echo -e "Creating 'initrd.tar' with files:"


# TODO: actually make an initrd :)

rm -rf initrd.dir
mkdir -p initrd.dir
pushd initrd.dir >/dev/null

echo "file one" > 1.txt
echo "file two" > 2.txt
mkdir -p dir
echo "file three" > dir/3.txt
# find . -type f -print | cpio -o > ../initrd.cpio

tar cvf ../initrd.tar *

popd >/dev/null



echo -e "Done.\n\n"

# reset the initrd.S
cat > initrd.S <<EOF
.section .initrd
mydata:
.incbin "initrd.tar"
EOF

$DIR/tc gcc -c -o initrd.o initrd.S || exit -1

# link the kernel again :^)
$DIR/tc ld -T kernel.ld $LDFLAGS --whole-archive root/lib/libchariot.a --no-whole-archive initrd.o $LIBGCC -o chariot_initrd.elf || exit -1

popd >/dev/null
popd >/dev/null

