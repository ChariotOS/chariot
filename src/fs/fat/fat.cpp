#include <fs/fat.h>
#include <lock.h>
#include <map.h>
#include <util.h>

#include "ff.h"
#include "ffconf.h"

// this exists to interface with the fat library I am using
static int next_fat_device = 0;  //
static mutex_lock fat_device_lock("fat_devices");
static map<int, ref<dev::device>> fat_devices;

fs::fat::fat(ref<dev::device> disk) {


  blocksize = disk->block_size();

  fat_device_lock.lock();
  device_id = next_fat_device++;
  fat_devices[device_id] = disk;
  fat_device_lock.unlock();

  fs = new FATFS();

}

fs::fat::~fat(void) {
  // remove the device from the global table
  fat_device_lock.lock();
  fat_devices.remove(device_id);
  fat_device_lock.unlock();

  if (fs != nullptr) {
    delete fs;
  }
}

bool is_zeros(char* b, int len) {
  for (int i = 0; i < len; i++) {
    if (b[i] != 0) return false;
  }
  return true;
}

bool fs::fat::init(void) {
  char buf[512];
  auto& disk = fat_devices[device_id];

  disk->read(0 /*offset*/, disk->block_size() /*size*/, buf /*dest*/);
  hexdump(buf, disk->block_size(), 16);

  char path[50];

  sprintk(path, "%d:", device_id);


  auto res = f_mount(fs, path, 1);

  if (res == FR_OK) {
    // it was okay!
    return true;
  }

  return false;
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
  printk("%s(%d)\n", __FUNCTION__, pdrv);
  //
  return 0;
}

extern "C" u8 fat_disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
  int res = 1; // error
  printk("%s(%d, %p, %d, %d)\n", __FUNCTION__, pdrv, buff, sector, count);
  fat_device_lock.lock();
  auto dev = fat_devices[pdrv];

  // fail if there isn't a device
  if (!dev) goto done;
  dev->read(sector * 512, count * 512, buff);
  res = 0; // OKAY

  hexdump(buff, 512, 16);

done:

  fat_device_lock.unlock();
  return res;
}

extern "C" u8 fat_disk_write(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
  int res = 1; // error
  printk("%s(%d, %p, %d, %d)\n", __FUNCTION__, pdrv, buff, sector, count);
  fat_device_lock.lock();
  auto dev = fat_devices[pdrv];

  // fail if there isn't a device
  if (!dev) goto done;
  dev->write(sector * 512, count * 512, buff);
  res = 0; // OKAY

done:

  fat_device_lock.unlock();
  return res;
}

extern "C" u8 fat_disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
  printk("%s(%d, %02x, %p)\n", __FUNCTION__, pdrv, cmd, buff);
  return 0;
}









// fat inode stuff
fs::fat_inode::fat_inode(fs::fat &fs, FIL *file) : fs::vnode(0), file(file), m_fs(fs) {

}

fs::fat_inode::~fat_inode(void) {

}

int fs::fat_inode::add_dir_entry(ref<vnode> node, const string &name, u32 mode) {
  return -ENOTIMPL;
}

ssize_t fs::fat_inode::read(filedesc &, void *, size_t) {
  return -ENOTIMPL;
}

ssize_t fs::fat_inode::write(filedesc &, void *, size_t) {
  return -ENOTIMPL;
}

int fs::fat_inode::truncate(size_t newlen) {
  return -ENOTIMPL;
}


fs::file_metadata fs::fat_inode::metadata(void) {
  return {};
}

bool fs::fat_inode::walk_dir_impl(func<bool(const string &, u32)> cb) {
  return false;
}
