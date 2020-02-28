#pragma once

#include <dev/device.h>
#include <fs.h>
#include <ptr.h>
#include <string.h>
#include <types.h>

#define MAX_DRIVERS 255

namespace dev {

// a driver is an interface to opening devices using a major
// number and a minor number
class driver : public refcounted<driver> {
 public:
  driver();
  virtual ~driver();

  ref<dev::device> open(major_t, minor_t);
  virtual ref<dev::device> open(major_t, minor_t, int &errcode);

  virtual int release(dev::device *);

  inline virtual ssize_t read(minor_t, fs::file &fd, void *buf, size_t sz) {
    return -1;
  }
  inline virtual ssize_t write(minor_t, fs::file &fd, const void *buf,
                               size_t sz) {
    return -1;
  }

  inline virtual int ioctl(fs::file &fd, int req, size_t arg) { return -1; }

  inline virtual const char *name(void) const { return "unknown"; }
};

// for use in register_driver
#define CHAR_DRIVER 1
#define BLOCK_DRIVER 2


int register_driver(const char *name, int type, major_t major,
                    fs::file_operations *operations);
int deregister_driver(major_t major);

int register_name(string name, major_t major, minor_t minor);
int deregister_name(string name);


/* return the next diskN where n is the next disk number
 * ie: disk1, disk2, disk3
 */
string next_disk_name(void);

// useful functions for the kernel to access devices by name or maj/min
fs::file open(string name);
fs::file open(major_t, minor_t);
fs::file open(major_t, minor_t, int &errcode);

fs::file_operations *get(major_t);

};  // namespace dev
