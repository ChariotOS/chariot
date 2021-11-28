# ChariotOS

Chariot is a simple unix-like kernel and userspace designed to easily play
around in if you are interested in kernel development. It currently supports
x86 and RISC-V, and "compiles" for ARM64 (help wanted). It's written in C++
and, with the exception of a few ports for my sanity, is entirely contained
in this repo.

If you are interested in helping out with development, feel free to file a PR!
I'd love all the help I can get.

### Building

Building the kernel currently has quite a number of host dependencies. A
(likely incomplete) list follows:

```
cmake
make
ninja
python3
nasm
grub2
```

Once you have those, building the kernel from scratch takes a few
steps (subsequent builds are little more than `make`).

First, you need to configure the kernel. Let's assume building for x86,
just run the command `make menuconfig` and exit. That will configure the
kernel with the default, sane configuration.

Once you do that, you will need to build a toolchain.
```bash
$ cd toolchain
$ ./build.sh x86_64
```
This command will build binutils and gcc for cross compilation. If you
want to build for RISC-V, replace "x86_64" in the build command with riscv64.

Once you have that built, go back to the root of the project and build
some of the required ports (freetype):
```bash
$ cd ports
$ ./build.sh x86_64
$ cd ../
```

Now, you can build the kernel. `make` will build it to `build/chariot.elf`, 
and you can run `tools/sync.sh` to create a bootable disk image at `build/chariot.img`.
You can of course run the kernel in QEMU with `tools/run.sh`. All additional options
to this script are passed through to QEMU, so I typically run with `tools/run.sh -nographic` when
doing development.

Feel free to file any issues if anything doesn't work!
