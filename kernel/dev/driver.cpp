#include <dev/driver.h>
#include <errno.h>
#include <map.h>
#include <printk.h>
#include <fs/vfs.h>

#define DRIVER_DEBUG

#ifdef DRIVER_DEBUG
#define INFO(fmt, args...) printk("[DRIVER] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

static rwlock drivers_lock;
static map<major_t, struct dev::driver_info *> drivers;
static map<string, dev_t> device_names;

int dev::register_driver(struct dev::driver_info &info) {
  assert(info.major != -1);
  assert(info.type == DRIVER_CHAR || info.type == DRIVER_BLOCK);
  assert(info.type == DRIVER_CHAR ? info.char_ops != NULL
				  : info.block_ops != NULL);

  drivers_lock.write_lock();

  drivers.set(info.major, &info);

  drivers_lock.write_unlock();

  return 0;
}

extern void devfs_register_device(string name, int type, int major, int minor);
int dev::register_name(struct dev::driver_info &info, string name,
		       minor_t min) {
  // create the block device if it is one
  if (info.type == DRIVER_BLOCK) {
    auto bdev = new fs::blkdev(dev_t(info.major, min), name, *info.block_ops);
    fs::blkdev::acquire(bdev);
    info.block_devices[min] = bdev;
  }

  drivers_lock.write_lock();

  printk(KERN_INFO "register name %s [maj:%d, min:%d] (%d total)\n", name.get(),
	 info.major, min, device_names.size());

  device_names.set(name, dev_t(info.major, min));
  drivers_lock.write_unlock();

	int ftype = info.type == DRIVER_BLOCK ? T_BLK : T_CHAR;

	devfs_register_device(name, ftype, info.major, min);

  return 0;
}

int dev::deregister_name(struct dev::driver_info &, string name) {
  drivers_lock.write_lock();
  device_names.remove(name);
  drivers_lock.write_unlock();
  return -ENOENT;
}

void dev::populate_inode_device(fs::inode &ino) {
  if (drivers.contains(ino.major)) {
    auto *d = drivers[ino.major];
    d->lock.read_lock();

    if (d->type == DRIVER_BLOCK) {
      ino.fops = &fs::block_file_ops;

      ino.blk.dev = d->block_devices[ino.minor];
      fs::blkdev::acquire(ino.blk.dev);
    }

    if (d->type == DRIVER_CHAR) {
      ino.fops = d->char_ops;
    }
    d->lock.read_unlock();
  }
}

struct fs::inode *devicei(struct dev::driver_info &d, string name,
			  minor_t min) {
  fs::inode *ino =
      new fs::inode(d.type == DRIVER_BLOCK ? T_BLK : T_CHAR, fs::DUMMY_SB);

  ino->major = d.major;
  ino->minor = min;

  dev::populate_inode_device(*ino);

  return ino;
}

ref<fs::file> dev::open(string name) {
  if (device_names.contains(name)) {
    auto dev = device_names.get(name);
    int maj = dev.major();
    int min = dev.minor();

    drivers_lock.read_lock();

    // TODO: take a lock!
    if (drivers.contains(maj)) {
      auto *d = drivers[maj];

      string path = string::format("/dev/%s", name.get());

      auto ino = devicei(*d, name, min);
      return fs::file::create(ino, path, FDIR_READ | FDIR_WRITE);
    }
  }

  return nullptr;
}

static int disk_count = 0;
string dev::next_disk_name(void) {
  return string::format("disk%d", disk_count);
}

/**
 * look up a device
 */
struct fs::blkdev *fs::bdev_from_path(const char *n) {
  struct fs::blkdev *bdev = nullptr;

	// if we don't have root yet, we need to emulate devfs
	if (vfs::get_root() == NULL) {

  string name = n;

  auto parts = name.split('/');

  if (parts.size() != 2 || parts[0] != "dev") {
    panic("invalid blkdev path %s\n", n);
  }


  drivers_lock.read_lock();
	if (device_names.contains(parts[1])) {
		dev_t d = device_names.get(parts[1]);
		// pretty sure the two maps should always be in sync
		auto *driver = drivers.get(d.major());
		if (driver != NULL) {
			if (driver->type == DRIVER_BLOCK) {
				bdev = driver->block_devices[d.minor()];
			}
		}
	}
  drivers_lock.read_unlock();
	}


  return bdev;
}
