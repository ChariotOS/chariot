#include <fs/fat.h>
#include <fs/vnode.h>
#include <lock.h>
#include <map.h>
#include <util.h>

#include "ff.h"
#include "ffconf.h"

// this exists to interface with the fat library I am using
static int next_fat_device = 0;  //
static mutex_lock fat_device_lock("fat_devices");
static map<int, ref<dev::device>> fat_devices;

/**
 * the concept of an inode id is very foreign to FAT, so I have to cram it on
 * top by generating new inode numbers in an increasing order, then maintaining
 * a pointer to the file object
 */
class fat_inode : public fs::vnode {
  friend class ext2;

 public:
  explicit fat_inode(int index, fs::fat &fs, FIL *file)
      : fs::vnode(index), file(file), m_fs(fs) {
    is_file = 1;
  }

  explicit fat_inode(int index, fs::fat &fs, DIR *dir)
      : fs::vnode(index), dir(dir), m_fs(fs) {
    is_file = 0;
  }

  virtual ~fat_inode() {}

  virtual fs::file_metadata metadata(void) { return {}; }

  virtual int add_dir_entry(ref<vnode> node, const string &name, u32 mode) {
    return -ENOTIMPL;
  }

  virtual ssize_t read(fs::filedesc &, void *, size_t) { return -ENOTIMPL; }

  virtual ssize_t write(fs::filedesc &, void *, size_t) { return -ENOTIMPL; }

  virtual int truncate(size_t newlen) { return -ENOTIMPL; }

  // return the block size of the fs
  int block_size();

  inline virtual fs::filesystem &fs() { return m_fs; }

  int is_file = 0;
  FIL *file = nullptr;
  DIR *dir = nullptr;

 protected:
  int cached_path[4] = {0, 0, 0, 0};
  int *blk_bufs[4] = {NULL, NULL, NULL, NULL};

  ssize_t do_rw(fs::filedesc &, size_t, void *, bool is_write);
  off_t block_for_byte(off_t b);

  virtual bool walk_dir_impl(func<bool(const string &, u32)> cb) {
    return false;
  }

  fs::filesystem &m_fs;
};

//////////////////////
//////////////////////
//////////////////////
//////////////////////
//////////////////////
//////////////////////
//////////////////////
//////////////////////
//////////////////////

fs::fat::fat(ref<dev::device> disk) {
  blocksize = disk->block_size();

  fat_device_lock.lock();
  device_id = next_fat_device++;
  fat_devices[device_id] = disk;
  fat_device_lock.unlock();
}

fs::fat::~fat(void) {
  // remove the device from the global table
  fat_device_lock.lock();
  fat_devices.remove(device_id);

  // unmount the disk
  char path[10];
  sprintk(path, "%d:", device_id);
  f_mount(0, path, 0);

  fat_device_lock.unlock();
}

bool is_zeros(char *b, int len) {
  for (int i = 0; i < len; i++) {
    if (b[i] != 0) return false;
  }
  return true;
}

bool fs::fat::init(void) {
  char path[50];

  sprintk(path, "%d:", device_id);

  auto res = f_mount(&fs, path, 1);

  if (res != FR_OK) {
    // it was bad!
    return false;
  }

  // allocate space for the dir node!
  auto root = new DIR;

  sprintk(path, "%d:", device_id);
  f_opendir(root, path);

  FILINFO fno;
  // rewind
  f_readdir(root, NULL);
  while (1) {
    f_readdir(root, &fno);
    if (fno.fname[0] == 0) break;


    printk("%s\n", fno.fname);
  }


  return true;
}

fs::vnoderef fs::fat::get_inode(u32 index) {
  printk("get inode: %d\n", index);
  // TODO
  return nullptr;
}

fs::vnoderef fs::fat::get_root_inode(void) {
  printk("get_root\n");
  // TODO
  return nullptr;
}

extern "C" DWORD get_fattime(void) { return 0; }

extern "C" u8 fat_disk_initialize(BYTE pdrv  // Physical drive nmuber
) {
  printk("%s(%d)\n", __FUNCTION__, pdrv);
  return 0;
}

extern "C" u8 fat_disk_status(BYTE pdrv) {
  // printk("%s(%d)\n", __FUNCTION__, pdrv);
  //
  return 0;
}

extern "C" u8 fat_disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
  int res = 1;  // error
  // printk("%s(%d, %p, %d, %d)\n", __FUNCTION__, pdrv, buff, sector, count);
  fat_device_lock.lock();
  auto dev = fat_devices[pdrv];

  // fail if there isn't a device
  if (!dev) goto done;
  dev->read(sector * 512, count * 512, buff);
  res = 0;  // OKAY

  // hexdump(buff, 512, 16);

done:

  fat_device_lock.unlock();
  return res;
}

extern "C" u8 fat_disk_write(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
  int res = 1;  // error
  // printk("%s(%d, %p, %d, %d)\n", __FUNCTION__, pdrv, buff, sector, count);
  fat_device_lock.lock();
  auto dev = fat_devices[pdrv];

  // fail if there isn't a device
  if (!dev) goto done;
  dev->write(sector * 512, count * 512, buff);
  res = 0;  // OKAY

done:

  fat_device_lock.unlock();
  return res;
}

extern "C" u8 fat_disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
  // printk("%s(%d, %02x, %p)\n", __FUNCTION__, pdrv, cmd, buff);
  return 0;
}

// fat inode stuff

