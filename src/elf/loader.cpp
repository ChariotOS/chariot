#include <elf/loader.h>
#include <printk.h>

elf::loader::loader(u8 *buffer, int size) : image(buffer, size) {
}
