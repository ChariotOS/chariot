#ifndef __VIRTIO_MMIO_H__
#define __VIRTIO_MMIO_H__

#include <types.h>
#include <asm.h>

typedef u32 mreg;


/**
 * struct virtio_mmio_regs
 *
 * represents the register layout of a virtio_mmio MMIO region
 */
struct virtio_mmio_regs {
  /*
   * 0x7472697 (a Little Endian equivalent of the “virt” string).
   */
  mreg magic_value;
  /*
   * 0x2
   */
  mreg version;

  /*
   * Virtio Subsystem Device ID
   * See 5 Device Types for possible values. Value zero (0x0) is used to de-
   * fine a system memory map with placeholder devices at static, well known
   * addresses, assigning functions to them depending on user’s needs.
   */
  mreg device_id;
  /*
   * Virtio Subsystem Vendor ID
   */
  mreg vendor_id;
  /*
   * Flags representing features the device supports
   *
   * Reading from this register returns 32 consecutive flag bits, the least
   * signifi- cant bit depending on the last value written to DeviceFeaturesSel.
   * Access to this register returns bits DeviceFeaturesSel ∗ 32 to
   * (DeviceFeaturesSel ∗ 32)+31, eg. feature bits 0 to 31 if DeviceFeaturesSel
   * is set to 0 and features bits 32 to 63 if DeviceFeaturesSel is set to 1.
   * Also see 2.2 Feature Bits.
   */
  mreg device_features;
  /*
   * Device (host) features word selection.
   *
   * Writing to this register selects a set of 32 device feature bits accessible
   * by reading from DeviceFeatures.
   */
  mreg device_features_sel;

  /*
   * Flags representing device features understood and activated by the driver
   *
   * Writing to this register sets 32 consecutive flag bits, the least
   * significant bit depending on the last value written to DriverFeaturesSel.
   * Access to this register sets bits DriverFeaturesSel ∗ 32 to
   * (DriverFeaturesSel ∗ 32) + 31, eg. feature bits 0 to 31 if
   * DriverFeaturesSel is set to 0 and features bits 32 to 63 if
   * DriverFeaturesSel is set to 1. Also see 2.2 Feature Bits.
   */
  mreg driver_features;
  /*
   * Activated (guest) features word selection
   *
   * Writing to this register selects a set of 32 activated feature bits
   * accessible by writing to DriverFeatures.
   */
  mreg driver_features_sel;

  /*
   * Virtual queue index
   *
   * Writing to this register selects the virtual queue that the following op-
   * erations on QueueNumMax, QueueNum, QueueReady, QueueDescLow, QueueDescHigh,
   * QueueAvailLow, QueueAvailHigh, QueueUsedLow and QueueUsedHigh apply to. The
   * index number of the first queue is zero (0x0).
   */
  mreg queue_sel;

  /*
   * Maximum virtual queue size
   *
   * Reading from the register returns the maximum size (number of elements) of
   * the queue the device is ready to process or zero (0x0) if the queue is not
   * available. This applies to the queue selected by writing to QueueSel.
   */
  mreg queue_num_max;

  /*
   * Virtual queue size
   *
   * Queue size is the number of elements in the queue. Writing to this register
   * notifies the device what size of the queue the driver will use. This
   * applies to the queue selected by writing to QueueSel.
   */
  mreg queue_num;

  /*
   * Virtual queue ready bit
   *
   * Writing one (0x1) to this register notifies the device that it can execute
   * re- quests from this virtual queue. Reading from this register returns the
   * last value written to it. Both read and write accesses apply to the queue
   * selected by writing to QueueSel.
   */
  mreg queue_ready;

  /*
   * Queue notifier
   *
   * Writing a value to this register notifies the device that there are new
   * buffers to process in a queue. When VIRTIO_F_NOTIFICATION_DATA has not been
   * negotiated, the value written is the queue index. When
   * VIRTIO_F_NOTIFICATION_DATA has been negotiated, the Notifi- cation data
   * value has the following format:
   */
  mreg queue_notify;

  /*
   * Interrupt status
   *
   * Reading from this register returns a bit mask of events that caused the de-
   * vice interrupt to be asserted. The following events are possible: Used
   * Buffer Notification - bit 0 - the interrupt was asserted because the device
   *    has used a buffer in at least one of the active virtual queues.
   * ConfigurationChangeNotification -bit1-theinterrupt was asserted because the
   * configuration of the device has changed.
   */
  mreg interrupt_status;

  /*
   * Interrupt acknowledge
   *
   * Writing a value with bits set as defined in InterruptStatus to this
   * register notifies the device that events causing the interrupt have been
   * handled.
   */
  mreg interrupt_ack;

  /*
   * Device status
   *
   * Reading from this register returns the current device status flags. Writing
   * non-zero values to this register sets the status flags, indicating the
   * driver progress. Writing zero (0x0) to this register triggers a device
   * reset. See also p. 4.2.3.1 Device Initialization.
   */
  mreg status;

  /*
   * Virtual queue’s Descriptor Area 64 bit long physical address
   *
   * Writing to these two registers (lower 32 bits of the address to
   * QueueDescLow, higher 32 bits to QueueDescHigh) notifies the device about
   * location of the Descriptor Area of the queue selected by writing to
   * QueueSel register.
   */
  mreg queue_desc_lo;
  mreg queue_desc_hi;

  /*
   * Virtual queue’s Driver Area 64 bit long physical address
   *
   * Writing to these two registers (lower 32 bits of the address to QueueAvail-
   * Low, higher 32 bits to QueueAvailHigh) notifies the device about location
   * of the Driver Area of the queue selected by writing to QueueSel.
   */
  mreg queue_driver_lo;
  mreg queue_driver_hi;

  /*
   * Virtual queue’s Device Area 64 bit long physical address
   *
   * Writing to these two registers (lower 32 bits of the address to QueueUsed-
   * Low, higher 32 bits to QueueUsedHigh) notifies the device about location of
   * the Device Area of the queue selected by writing to QueueSel.
   */
  mreg queue_device_lo;
  mreg queue_device_hi;

  /*
   * Configuration atomicity value
   *
   * Reading from this register returns a value describing a version of the
   * device- specific configuration space (see Config). The driver can then
   * access the configuration space and, when finished, read ConfigGeneration
   * again. If no part of the configuration space has changed between these two
   * ConfigGen- eration reads, the returned values are identical. If the values
   * are different, the configuration space accesses were not atomic and the
   * driver has to perform the operations again. See also 2.4.
   */
  mreg config_generation;

  /*
   * Configuration space
   *
   * Device-specific configuration space starts at the offset 0x100 and is ac-
   * cessed with byte alignment. Its meaning and size depend on the device and
   * the driver.
   */
  mreg config[0];
} __packed;

#endif
