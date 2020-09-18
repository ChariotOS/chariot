# ChariotOS
## A simple unix-like kernel written in C++


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
