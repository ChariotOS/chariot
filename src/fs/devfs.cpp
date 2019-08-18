#include <fs/devfs.h>
#include <dev/device.h>
#include <vec.h>
#include <map.h>


// a list of all the devices on the system.
// TODO: turn this into a hashmap of the names
static map<string, unique_ptr<dev::device>> devices;

void fs::devfs::register_device(string name, unique_ptr<dev::device> dev) {
  // TODO LOCK GLOBAL DEVICE LIST
  devices[name] = move(dev);
}


bool fs::devfs::device_exists(string name) {
  for (auto &e : devices) {
    if (e.key == name) {
      return true;
    }
  }
  return false;
}



dev::device& fs::devfs::get_device(string name) {

  if (devices.contains(name)) {
    return *devices[name];
  }
  panic("check if a device exists before calling get_device. Failed when looking up '%s'\n", name.get());

  // bogus
  return get_device("");
}



