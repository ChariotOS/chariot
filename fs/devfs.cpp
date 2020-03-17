#include <fs/devfs.h>
#include <module.h>


static void devfs_init(void) {
}


module_init("fs::devfs", devfs_init);
