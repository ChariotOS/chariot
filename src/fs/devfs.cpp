#include <dev/device.h>
#include <fs/devfs.h>
#include <map.h>
#include <vec.h>
#include <dev/mbr.h>

// a list of all the devices on the system.
// TODO: turn this into a hashmap of the names
static map<string, unique_ptr<dev::device>> devices;

void fs::devfs::register_device(string name, unique_ptr<dev::device> dev,
                                u32 flags) {

  printk("[DEVFS] registering %s\n", name.get());
  if (flags & DEVFS_REG_WALK_PARTS) {
    if (!dev->is_blk_device())
      panic("cannot walk partitions on a non-block device %s\n", name.get());

    // try MBR
    // TODO: add more partition schemes
    auto &bdev = *static_cast<dev::blk_dev*>(dev.get());

    dev::mbr parts(bdev);
    if (parts.parse()) {
      for_range(i, 0, parts.part_count()) {
        auto part = parts.partition(i);

        auto part_name = string::format("%ss%d", name.get(), i);

        register_device(part_name, move(part), flags);
      }
    }
  }


  // TODO LOCK GLOBAL DEVICE LIST
  devices[name] = move(dev);
}


dev::device* fs::devfs::get_device(string name) {
  if (devices.contains(name)) {
    return devices[name].get();
  }
  // device wasnt found, return null
  return nullptr;
}

