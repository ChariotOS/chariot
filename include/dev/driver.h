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

  inline virtual ssize_t read(minor_t, fs::filedesc &fd, void *buf, size_t sz) {
    return -1;
  }
  inline virtual ssize_t write(minor_t, fs::filedesc &fd, const void *buf,
                               size_t sz) {
    return -1;
  }

  inline virtual int ioctl(fs::filedesc &fd, int req, size_t arg) { return -1; }

  inline virtual const char *name(void) const { return "unknown"; }
};

// for use in register_driver
#define CHAR_DRIVER 1
#define BLOCK_DRIVER 2

/**
 * linux-style device driver operations
 */
struct driver_ops {
  ssize_t (*llseek)(fs::filedesc &, ssize_t, int) = NULL;
  ssize_t (*read)(fs::filedesc &, char *, size_t) = NULL;
  ssize_t (*write)(fs::filedesc &, const char *, size_t) = NULL;
  int (*ioctl)(fs::filedesc &, unsigned int, unsigned long) = NULL;

  /**
   * open - notify the driver that a new descriptor has opened the file
   * if it returns a non-zero code, it will be propagated back to the
   * sys::open() call and be considered a fail
   */
  int (*open)(fs::filedesc &) = NULL;

  /**
   * close - notify the driver that a file descriptor is closing it
   *
   * no return value
   */
  void (*close)(fs::filedesc &) = NULL;
  /*
    int (*readdir) (struct file *, void *, filldir_t);
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    int (*mmap) (struct file *, struct vm_area_struct *);
    int (*flush) (struct file *);
    int (*fsync) (struct file *, struct dentry *, int datasync);
    int (*fasync) (int, struct file *, int);
    int (*lock) (struct file *, int, struct file_lock *);
    ssize_t (*readv) (struct file *, const struct iovec *, unsigned long,
      loff_t *);
    ssize_t (*writev) (struct file *, const struct iovec *, unsigned long,
      loff_t *);
            */
};

int register_driver(const char *name, int type, major_t major,
                    dev::driver_ops *operations);
int deregister_driver(major_t major);

int register_name(string name, major_t major, minor_t minor);
int deregister_name(string name);


/* return the next diskN where n is the next disk number
 * ie: disk1, disk2, disk3
 */
string next_disk_name(void);

// useful functions for the kernel to access devices by name or maj/min
fs::filedesc open(string name);
fs::filedesc open(major_t, minor_t);
fs::filedesc open(major_t, minor_t, int &errcode);

dev::driver_ops *get(major_t);

};  // namespace dev
