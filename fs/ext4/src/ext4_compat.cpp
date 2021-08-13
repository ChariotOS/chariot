#include <cpu.h>
#include <rbtree.h>
#include <module.h>
#include <dev/disk.h>
#include <fs.h>
#include <fs/vfs.h>
#include <errno.h>
#include <mem.h>
#include <syscall.h>

extern "C" {
#include <ext4_compat_stdlib.h>
}
#include <ext4.h>
#include <ext4_blockdev.h>


extern "C" void *ext4_user_malloc(unsigned long size) { return malloc(size); }
extern "C" void *ext4_user_calloc(unsigned long count, unsigned long size) {
  return zalloc(size * count);
}
extern "C" void *ext4_user_realloc(void *ptr, unsigned long size) { return realloc(ptr, size); }
extern "C" void ext4_user_free(void *ptr) { free(ptr); }


/******************************************************************************/
static int blockdev_open(struct ext4_blockdev *bdev) {
  /*blockdev_open: skeleton*/
  return 0;
}

/******************************************************************************/

static int blockdev_bread(
    struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt) {
  printk("blockdev_bread %d %d\n", blk_id, blk_cnt);

  auto *dev = (fs::blkdev *)bdev->bdif->p_user;
  for (int o = 0; o < blk_cnt; o++) {
    void *dst = (char *)buf + o * dev->block_size;
    dev->read_block(dst, blk_id + o);
  }
  /*blockdev_bread: skeleton*/
  return 0;
}


/******************************************************************************/
static int blockdev_bwrite(
    struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt) {
  printk("blockdev_bwrite %d %d\n", blk_id, blk_cnt);
  auto *dev = (fs::blkdev *)bdev->bdif->p_user;
  for (int o = 0; o < blk_cnt; o++) {
    void *dst = (char *)buf + o * dev->block_size;
    dev->write_block(dst, blk_id + o);
  }
  return 0;
}
/******************************************************************************/
static int blockdev_close(struct ext4_blockdev *bdev) {
  /*blockdev_close: skeleton*/
  return 0;
}

static int blockdev_lock(struct ext4_blockdev *bdev) {
  /*blockdev_lock: skeleton*/
  return 0;
}

static int blockdev_unlock(struct ext4_blockdev *bdev) {
  /*blockdev_unlock: skeleton*/
  return 0;
}



struct ext4_blockdev *ext4_wrap_blockdev(fs::blkdev *bdev) {
  auto *iface = new ext4_blockdev_iface;
  iface->open = blockdev_open;
  iface->bread = blockdev_bread;
  iface->bwrite = blockdev_bwrite;
  iface->close = blockdev_close;
  iface->lock = blockdev_lock;
  iface->unlock = blockdev_unlock;
  iface->p_user = (void *)bdev;

  iface->ph_bsize = bdev->block_size;
  iface->ph_bcnt = bdev->block_count;
  iface->ph_bbuf = new uint8_t[iface->ph_bsize];

  auto *ebdev = new ext4_blockdev;
  ebdev->bdif = iface;
  ebdev->part_offset = 0;
  ebdev->part_size = iface->ph_bsize * iface->ph_bcnt;

  return ebdev;
}



int ext4_sb_init(struct fs::superblock &sb) { return -ENOTIMPL; }

int ext4_write_super(struct fs::superblock &sb) { return -ENOTIMPL; }

int ext4_sync(struct fs::superblock &sb, int flags) { return -ENOTIMPL; }



static const char *entry_to_str(uint8_t type) {
  switch (type) {
    case EXT4_DE_UNKNOWN:
      return "[unk] ";
    case EXT4_DE_REG_FILE:
      return "[fil] ";
    case EXT4_DE_DIR:
      return "[dir] ";
    case EXT4_DE_CHRDEV:
      return "[cha] ";
    case EXT4_DE_BLKDEV:
      return "[blk] ";
    case EXT4_DE_FIFO:
      return "[fif] ";
    case EXT4_DE_SOCK:
      return "[soc] ";
    case EXT4_DE_SYMLINK:
      return "[sym] ";
    default:
      break;
  }
  return "[???]";
}

void dir_ls(const char *path) {
  char sss[255];
  ext4_dir d;
  const ext4_direntry *de;

  printk("ls %s\n", path);

  ext4_dir_open(&d, path);
  de = ext4_dir_entry_next(&d);

  while (de) {
    memcpy(sss, de->name, de->name_length);
    sss[de->name_length] = 0;
    printk("  %s%s\n", entry_to_str(de->inode_type), sss);
    de = ext4_dir_entry_next(&d);
  }
  ext4_dir_close(&d);
}



class ext4_sb : public fs::superblock {
 public:
  ext4_sb(void) {}
  ~ext4_sb(void) {}

  bool init(fs::blkdev *bdev) {
    fs::blkdev::acquire(bdev);
    this->bdev = bdev;
    disk = fs::bdev_to_file(bdev);
    this->bdev = bdev;
    this->sector_size = bdev->block_size;

    bd = ext4_wrap_blockdev(bdev);
    ext4_block_init(bd);

    ext4_dmask_set(DEBUG_ALL);

    int r;

    r = ext4_device_register(bd, "ext4_fs");
    if (r != EOK) {
      printk("ext4_device_register: rc = %d\n", r);
      return false;
    }

    r = ext4_mount("ext4_fs", "/mp/", false);
    if (r != EOK) {
      printk("ext4_mount: rc = %d\n", r);
      return false;
    }

    r = ext4_recover("/mp/");
    if (r != EOK && r != ENOTSUP) {
      printk("ext4_recover: rc = %d\n", r);
      return false;
    }

    r = ext4_journal_start("/mp/");
    if (r != EOK) {
      printk("ext4_journal_start: rc = %d\n", r);
      return false;
    }

    ext4_cache_write_back("/mp/", 1);
    dir_ls("/mp/");

    dir_ls("/mp/sys/fonts/Emoji");

    // dir_ls("/mp");



    return true;
  }

 private:
  struct ext4_blockdev *bd = nullptr;


  spinlock m_lock;
  ck::ref<fs::file> disk;
  fs::blkdev *bdev;
  long sector_size;
  ck::map<u32, struct fs::inode *> inodes;
};


struct fs::sb_operations ext4_ops {
  .init = ext4_sb_init, .write_super = ext4_write_super, .sync = ext4_sync,
};



static struct fs::superblock *ext4_mount(
    struct fs::sb_information *, const char *args, int flags, const char *device) {
  struct fs::blkdev *bdev = fs::bdev_from_path(device);
  if (bdev == NULL) return NULL;

  auto *sb = new ext4_sb();

  if (!sb->init(bdev)) {
    delete sb;
    return NULL;
  }

  return sb;
}

struct fs::sb_information ext4_info {
  .name = "ext4", .mount = ext4_mount, .ops = ext4_ops,
};

static void ext4_init(void) { vfs::register_filesystem(ext4_info); }

module_init("fs::ext4/ext3/ext4", ext4_init);
