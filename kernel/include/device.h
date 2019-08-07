#ifndef __DEVICE_H__
#define __DEVICE_H__


#include <types.h>

struct driver;
struct device_driver;
struct device;



enum device_type {
  DEV_T_UNKNOWN,
  DEV_T_BLK,
  DEV_T_CHAR,
};

/**
 * struct device_driver - the basic device driver structure
 *
 * @name: the name of the driver
 * @probe: called to query whether a device can work with this
 *         particular driver, and attach it to the device_driver
 *         @returns: true on success, false on any kind of failure
 *
 * @remove: called when the device is removed from the system to
 *         unbind it from this device driver
 *         @returns: true on removal, false if the device failed
 *                   to remove and/or if the device wasn't bound
 *                   to this driver
 */
struct device_driver {
  const char *name;

  bool (*probe)(struct device_driver *, struct device *);
  bool (*remove)(struct device_driver *, struct device *);

  void *private_data;
};


void assign_drivers();

#endif
