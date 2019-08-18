#include <dev/device.h>
#include <fs/filesystem.h>
#include <string.h>

namespace fs {

class devfs {
 public:
  /**
   * register a device under a certain name, passing ownership to devfs
   *Access hereon out is through `dev::device&`
   */
  static void register_device(string name, unique_ptr<dev::device>);

  static bool device_exists(string name);
  // WARNING: will panic when the device could not be found. Use after
  // devfs::device_exists every time unless writing code internal to
  // devfs. We could also implement exceptions to handle failures here,
  // but I dont really want to right now
  static dev::device& get_device(string name);
};

};  // namespace fs
