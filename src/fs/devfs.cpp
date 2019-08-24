#include <dev/device.h>
#include <dev/mbr.h>
#include <fs/devfs.h>
#include <map.h>
#include <vec.h>

// a list of all the devices on the system.
// TODO: turn this into a hashmap of the names
static map<string, unique_ptr<dev::device>> devices;

void fs::devfs::register_device(string name, unique_ptr<dev::device> dev,
                                u32 flags) {

  if (flags & DEVFS_REG_WALK_PARTS) {
    if (!dev->is_blk_device())
      panic("cannot walk partitions on a non-block device %s\n", name.get());

    // try MBR
    // TODO: add more partition schemes
    auto& bdev = *static_cast<dev::blk_dev*>(dev.get());

    dev::mbr parts(bdev);
    if (parts.parse()) {
      for_range(i, 0, parts.part_count()) {
        auto part = parts.partition(i);
        register_device(string::format("%sp%d", name.get(), i), move(part),
                        flags);
      }
    }
  }

  printk("[DEVFS] registering %s\n", name.get());

  // TODO LOCK GLOBAL DEVICE LIST
  devices.set(name, move(dev));
}

dev::device* fs::devfs::get_device(string name) {

  // TODO: LOCK THE DEVICE LIST
  if (devices.contains(name)) {
    return devices[name].get();
  }

  printk("nope\n");
  // device wasnt found, return null
  return nullptr;
}

