# ChariotOS

Chariot is a simple unix-like kernel and userspace designed to easily play
around in if you are interested in kernel development. It currently supports
x86 and RISC-V, and "compiles" for ARM (help wanted). It's written in C++
and, with the exception of a few ports for my sanity, is entirely contained
in this repo.

### How to build/run

To build the kernel, build an image, and run in QEMU, just run this command:
```
$ tools/run.sh
```

And to just build it into a binary and img:
```
$ tools/sync.sh
```
Or, if you're old fashioned, you can just run `make` :)


oh, and it runs doom

![It runs doom](https://github.com/nickwanninger/chariot/raw/trunk/meta/DOOM.png)

You just need to put your copy of doom1.wad in /usr/res/misc/doom1.wad and `/bin/doom` will run.
Either that or run `doom -iwad <path to wad>`
