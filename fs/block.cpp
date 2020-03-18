#include <errno.h>
#include <fs.h>

// ideally, the seek operation would never go out of sync, so this just checks
// that we only ever seek by blocksize amounts
static int blk_seek(fs::file &f, off_t o, off_t) {
  struct fs::blkdev *dev = f.ino->blk.dev;
  if (!dev) {
    return -EINVAL;
  }
  // make sure that o is a multiple of the block size
  if (o % dev->block_size != 0) {
    return -1;
  }
  return 0;  // allow seek
}

static ssize_t blk_rw(fs::file &f, char *data, size_t len, bool write) {
  size_t first_block, end_block, blk_count;
  ssize_t n = 0;
  off_t offset;
  struct fs::blkdev *dev;

  if (f.ino->type != T_BLK || !f.ino->blk.dev) return -EINVAL;

  dev = f.ino->blk.dev;
  offset = f.offset();

  if ((offset % dev->block_size) != 0 || (len % dev->block_size) != 0) {
    return -EINVAL;
  }

  first_block = offset / dev->block_size;
  end_block = (offset + len) / dev->block_size;
  blk_count = end_block - first_block;

  for (u64 i = 0; i < blk_count; i++) {
    auto res = dev->ops.rw_block(*dev, data + dev->block_size * i,
				 first_block + i, write);
    if (res != 0) {
      return -EIO;
    }
    n += dev->block_size;
  }
  f.seek(n, SEEK_CUR);
  return n;
}

static ssize_t blk_read(fs::file &f, char *data, size_t len) {
  return blk_rw(f, data, len, false);
}

static ssize_t blk_write(fs::file &f, const char *data, size_t len) {
  return blk_rw(f, (char *)data, len, true);
}

static int blk_ioctl(fs::file &f, unsigned int num, off_t val) {
  auto dev = f.ino->blk.dev;
  if (!dev || !dev->ops.ioctl) return -EINVAL;
  return dev->ops.ioctl(*dev, num, val);
}

struct fs::file_operations fs::block_file_ops {
  .seek = blk_seek, .read = blk_read, .write = blk_write, .ioctl = blk_ioctl,
};
