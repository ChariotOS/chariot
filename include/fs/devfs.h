#include <dev/device.h>
#include <fs/filesystem.h>
#include <string.h>

#define DEVFS_REG_WALK_PARTS (1 << 0)

namespace fs {

class devfs {
 public:
  /**
   * register a device under a certain name, passing ownership to devfs
   * Access here-on out is through `dev::device&`. walk_partitions will, on a
   * disk device, attempt to recursively read partitions from the device
   */
  static void register_device(string name, unique_ptr<dev::device>,
                              u32 flags = 0);

  // return the device at the name. Return null if it was not found.
  // NOTICE: DO NOT STORE THE RESULT OF THIS FUNCTION ANYWHERE IN SMART
  // POINTERS. ONLY USE THE RAW POINTER OR A REFERENCE WHEN IT IS DEEMED VALID
  static dev::device *get_device(string name);
};

};  // namespace fs
