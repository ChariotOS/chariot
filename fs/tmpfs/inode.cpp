#include <errno.h>
#include "tmpfs.h"


static ssize_t tfs_write(fs::file &f, const char *src, size_t sz) {
  UNIMPL();
  return -ENOTIMPL;
}


static ssize_t tfs_read(fs::file &f, char *dst, size_t sz) {
  UNIMPL();
  return -ENOTIMPL;
}


static int tfs_ioctl(fs::file &, unsigned int, off_t) {
  UNIMPL();
  return -ENOTIMPL;
}



static int tfs_seek(fs::file &, off_t, off_t) {
  UNIMPL();
  return 0;  // allow seek
}


static int tfs_open(fs::file &) {
  return 0;
}
static void tfs_close(fs::file &) {
}




static ref<mm::vmobject> tfs_mmap(fs::file &f, size_t npages, int prot, int flags, off_t off) {
  UNIMPL();
  // XXX: this is invalid, should be asserted before here :^)
  if (off & 0xFFF) return nullptr;
  return nullptr;
}



static int tfs_resize(fs::file &, size_t) {
  UNIMPL();
  return -ENOTIMPL;
}


/**
 * Free the tmp::file object in the inode
 */
static void tfs_destroy(fs::inode &v) {
  // printk(KERN_DEBUG "Destroy tmpfs inode\n");
  delete v.priv<tmp::priv>();
}

fs::file_operations tmp::fops = {
    .seek = tfs_seek,
    .read = tfs_read,
    .write = tfs_write,
    .ioctl = tfs_ioctl,
    .open = tfs_open,
    .close = tfs_close,
    .mmap = tfs_mmap,
    .resize = tfs_resize,
    .destroy = tfs_destroy,
};
