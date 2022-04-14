#ifndef uint_MMIO_H__
#define uint_MMIO_H__

#include <asm.h>
#include <types.h>
#include <dev/driver.h>

namespace virtio {


#define VIRTIO_MMIO_MAGIC_VALUE 0x000  // 0x74726976
#define VIRTIO_MMIO_VERSION 0x004      // version; 1 is legacy
#define VIRTIO_MMIO_DEVICE_ID 0x008    // device type; 1 is net, 2 is disk
#define VIRTIO_MMIO_VENDOR_ID 0x00c    // 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028   // page size for PFN, write-only
#define VIRTIO_MMIO_QUEUE_SEL 0x030         // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034     // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM 0x038         // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_ALIGN 0x03c       // used ring alignment, write-only
#define VIRTIO_MMIO_QUEUE_PFN 0x040         // physical page number for queue, read/write
#define VIRTIO_MMIO_QUEUE_READY 0x044       // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050      // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060  // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064     // write-only
#define VIRTIO_MMIO_STATUS 0x070            // read/write

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER 2
#define VIRTIO_CONFIG_S_DRIVER_OK 4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

#define VIRTIO_BLK_F_RO 5          /* Disk is read-only */
#define VIRTIO_BLK_F_SCSI 7        /* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE 11 /* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ 12         /* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT 27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX 29


  /**
   * struct virtio::mmio_regs
   *
   * represents the register layout of a virtio_mmio MMIO region
   */
  struct mmio_regs {
    /*
     * 0x7472697 (a Little Endian equivalent of the “virt” string).
     */
    volatile uint32_t magic_value;
    /*
     * 0x2
     */
    volatile uint32_t version;

    /*
     * Virtio Subsystem Device ID
     * See 5 Device Types for possible values. Value zero (0x0) is used to de-
     * fine a system memory map with placeholder devices at static, well known
     * addresses, assigning functions to them depending on user’s needs.
     */
    volatile uint32_t device_id;
    /*
     * Virtio Subsystem Vendor ID
     */
    volatile uint32_t vendor_id;
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
    volatile uint32_t device_features;
    /*
     * Device (host) features word selection.
     *
     * Writing to this register selects a set of 32 device feature bits accessible
     * by reading from DeviceFeatures.
     */
    volatile uint32_t device_features_sel;

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
    volatile uint32_t driver_features;
    /*
     * Activated (guest) features word selection
     *
     * Writing to this register selects a set of 32 activated feature bits
     * accessible by writing to DriverFeatures.
     */
    volatile uint32_t driver_features_sel;

    /*
     * Virtual queue index
     *
     * Writing to this register selects the virtual queue that the following op-
     * erations on QueueNumMax, QueueNum, QueueReady, QueueDescLow, QueueDescHigh,
     * QueueAvailLow, QueueAvailHigh, QueueUsedLow and QueueUsedHigh apply to. The
     * index number of the first queue is zero (0x0).
     */
    volatile uint32_t queue_sel;

    /*
     * Maximum virtual queue size
     *
     * Reading from the register returns the maximum size (number of elements) of
     * the queue the device is ready to process or zero (0x0) if the queue is not
     * available. This applies to the queue selected by writing to QueueSel.
     */
    volatile uint32_t queue_num_max;

    /*
     * Virtual queue size
     *
     * Queue size is the number of elements in the queue. Writing to this register
     * notifies the device what size of the queue the driver will use. This
     * applies to the queue selected by writing to QueueSel.
     */
    volatile uint32_t queue_num;

    /*
     * Virtual queue ready bit
     *
     * Writing one (0x1) to this register notifies the device that it can execute
     * re- quests from this virtual queue. Reading from this register returns the
     * last value written to it. Both read and write accesses apply to the queue
     * selected by writing to QueueSel.
     */
    volatile uint32_t queue_ready;

    /*
     * Queue notifier
     *
     * Writing a value to this register notifies the device that there are new
     * buffers to process in a queue. When VIRTIO_F_NOTIFICATION_DATA has not been
     * negotiated, the value written is the queue index. When
     * VIRTIO_F_NOTIFICATION_DATA has been negotiated, the Notifi- cation data
     * value has the following format:
     */
    volatile uint32_t queue_notify;

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
    volatile uint32_t interrupt_status;

    /*
     * Interrupt acknowledge
     *
     * Writing a value with bits set as defined in InterruptStatus to this
     * register notifies the device that events causing the interrupt have been
     * handled.
     */
    volatile uint32_t interrupt_ack;

    /*
     * Device status
     *
     * Reading from this register returns the current device status flags. Writing
     * non-zero values to this register sets the status flags, indicating the
     * driver progress. Writing zero (0x0) to this register triggers a device
     * reset. See also p. 4.2.3.1 Device Initialization.
     */
    volatile uint32_t status;

    /*
     * Virtual queue’s Descriptor Area 64 bit long physical address
     *
     * Writing to these two registers (lower 32 bits of the address to
     * QueueDescLow, higher 32 bits to QueueDescHigh) notifies the device about
     * location of the Descriptor Area of the queue selected by writing to
     * QueueSel register.
     */
    volatile uint32_t queue_desc_lo;
    volatile uint32_t queue_desc_hi;

    /*
     * Virtual queue’s Driver Area 64 bit long physical address
     *
     * Writing to these two registers (lower 32 bits of the address to QueueAvail-
     * Low, higher 32 bits to QueueAvailHigh) notifies the device about location
     * of the Driver Area of the queue selected by writing to QueueSel.
     */
    volatile uint32_t queue_driver_lo;
    volatile uint32_t queue_driver_hi;

    /*
     * Virtual queue’s Device Area 64 bit long physical address
     *
     * Writing to these two registers (lower 32 bits of the address to QueueUsed-
     * Low, higher 32 bits to QueueUsedHigh) notifies the device about location of
     * the Device Area of the queue selected by writing to QueueSel.
     */
    volatile uint32_t queue_device_lo;
    volatile uint32_t queue_device_hi;

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
    volatile uint32_t config_generation;

    /*
     * Configuration space
     *
     * Device-specific configuration space starts at the offset 0x100 and is ac-
     * cessed with byte alignment. Its meaning and size depend on the device and
     * the driver.
     */
    volatile uint32_t config[0];
  } __packed;

  /* The config space for a block device */
  struct blk_config {
    /* The capacity (in 512-byte sectors). */
    uint64_t capacity;
    /* The maximum segment size (if VIRTIO_BLK_F_SIZE_MAX) */
    uint32_t size_max;
    /* The maximum number of segments (if VIRTIO_BLK_F_SEG_MAX) */
    uint32_t seg_max;
    /* geometry of the device (if VIRTIO_BLK_F_GEOMETRY) */
    struct virtio_blk_geometry {
      uint16_t cylinders;
      uint8_t heads;
      uint8_t sectors;
    } geometry;

    /* block size of device (if VIRTIO_BLK_F_BLK_SIZE) */
    uint32_t blk_size;

    /* the next 4 entries are guarded by VIRTIO_BLK_F_TOPOLOGY  */
    /* exponent for physical block per logical block. */
    uint8_t physical_block_exp;
    /* alignment offset in logical blocks. */
    uint8_t alignment_offset;
    /* minimum I/O size without performance penalty in logical blocks. */
    uint16_t min_io_size;
    /* optimal sustained I/O size in logical blocks. */
    uint32_t opt_io_size;

    /* writeback mode (if VIRTIO_BLK_F_CONFIG_WCE) */
    uint8_t wce;
    uint8_t unused;

    /* number of vqs, only available when VIRTIO_BLK_F_MQ is set */
    uint16_t num_queues;

    /* the next 3 entries are guarded by VIRTIO_BLK_F_DISCARD */
    /*
     * The maximum discard sectors (in 512-byte sectors) for
     * one segment.
     */
    uint32_t max_discard_sectors;
    /*
     * The maximum number of discard segments in a
     * discard command.
     */
    uint32_t max_discard_seg;
    /* Discard commands must be aligned to this number of sectors. */
    uint32_t discard_sector_alignment;

    /* the next 3 entries are guarded by VIRTIO_BLK_F_WRITE_ZEROES */
    /*
     * The maximum number of write zeroes sectors (in 512-byte sectors) in
     * one segment.
     */
    uint32_t max_write_zeroes_sectors;
    /*
     * The maximum number of segments in a write zeroes
     * command.
     */
    uint32_t max_write_zeroes_seg;
    /*
     * Set if a VIRTIO_BLK_T_WRITE_ZEROES request may result in the
     * deallocation of one or more of the sectors.
     */
    uint8_t write_zeroes_may_unmap;

    uint8_t unused1[3];
  } __attribute__((packed));



#define NUM_DESC 8

  // a single descriptor, from the spec.
  struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
  };
#define VRING_DESC_F_NEXT 1   // chained with another descriptor
#define VRING_DESC_F_WRITE 2  // device writes (vs read)

  // the (entire) avail ring, from the spec.
  struct virtq_avail {
    uint16_t flags;           // always zero
    uint16_t idx;             // driver will write ring[idx] next
    uint16_t ring[NUM_DESC];  // descriptor numbers of chain heads
    uint16_t unused;
  };

  // one entry in the "used" ring, with which the
  // device tells the driver about completed requests.
  struct virtq_used_elem {
    uint32_t id;  // index of start of completed descriptor chain
    uint32_t len;
  };

  struct virtq_used {
    uint16_t flags;  // always zero
    uint16_t idx;    // device increments when it adds a ring[] entry
    struct virtq_used_elem ring[NUM_DESC];
  };



#define VIRTIO_BLK_T_IN 0   // read the disk
#define VIRTIO_BLK_T_OUT 1  // write the disk

  // the format of the first descriptor in a disk request.
  // to be followed by two more descriptors containing
  // the block, and a one-byte status.
  struct blk_req {
    uint32_t type;  // VIRTIO_BLK_T_IN or ..._OUT
    uint32_t reserved;
    uint64_t sector;
  } __packed;



  /* Given an address, check it for MMIO magic numbers */
  int check_mmio(void *addr, int irq);

}  // namespace virtio


#define VIO_PGCOUNT 2
#define VIO_NUM_DESC 8
#define VIO_MAX_RINGS 4


struct vring {
  bool active = false;
  uint32_t num;
  uint32_t num_mask;

  uint16_t free_list; /* head of a free list of descriptors per ring. 0xffff is NULL */
  uint16_t free_count;

  uint16_t last_used;

  struct virtio::virtq_desc *desc;
  struct virtio::virtq_avail *avail;
  struct virtio::virtq_used *used;
};


struct virtio_config {
  int irqnr; /* The IRQ number for this device */
             /* TODO: more config :) */
  volatile uint32_t *regs;
};


class VirtioMMIOVring {
 protected:
  vring ring[VIO_MAX_RINGS];
  volatile uint32_t *regs = NULL;

  inline VirtioMMIOVring(volatile uint32_t *regs) : regs(regs) {}

 public:
  struct Descriptor {
    uint64_t addr;
    uint32_t len;
    uint16_t flags = 0;

    template <typename T>
    Descriptor(T *addr, size_t sz, uint16_t flags = 0) : addr((uint64_t)v2p(addr)), len(sz), flags(flags) {}
  };
  int alloc_ring(int index, int len);

  // find a free descriptor, mark it non-free, return its index.
  uint16_t alloc_desc(int ring_index);

  // mark a descriptor as free.
  void free_desc(int ring_index, int i);
  void free_desc_chain(int ring_index, int i);
  void submit_chain(int ring_index, int desc_index);

  // abstract away allocating ring entries and desc entries.
  // Return 0 on success. ERRNO on error
  int submit(size_t ndescs, const Descriptor *descs, uint16_t first_index);
  int submit(const ck::vec<Descriptor> &descs, uint16_t first_index) { return submit(descs.size(), descs.data(), first_index); }


  virtio::virtq_desc *alloc_desc_chain(int ring_index, int count, uint16_t *start_index);
  inline virtio::virtq_desc *index_to_desc(int ring_index, int index) {
    auto d = &ring[ring_index].desc[index];
    return d;
  }


  inline auto &mmio_regs(void) { return *(virtio::mmio_regs *)regs; }
  inline uint32_t read_reg(int off) { return *(volatile uint32_t *)((off_t)regs + off); }
  inline void write_reg(int off, uint32_t val) { *(volatile uint32_t *)((off_t)regs + off) = val; }


  inline void kick(int ring) {
    __sync_synchronize();
    write_reg(VIRTIO_MMIO_QUEUE_NOTIFY, ring);
    __sync_synchronize();
  }
};


template <typename BaseDeviceT>
class VirtioMMIO : public BaseDeviceT, public VirtioMMIOVring {
 protected:
  virtio_config config;

 public:
  using BaseDevice = BaseDeviceT;

  template <typename... As>
  VirtioMMIO(virtio_config &cfg, As &...rest) : BaseDevice(rest...), VirtioMMIOVring(cfg.regs), config(cfg) {}
  virtual ~VirtioMMIO(void) {}


  virtual void handle_used(int ring_index, virtio::virtq_used_elem *) {}
  virtual bool initialize(void) = 0;

  // ^BaseDevice::
  void irq(int nr) override final;
};


template <typename T>
inline void VirtioMMIO<T>::irq(int nr) {
  write_reg(VIRTIO_MMIO_INTERRUPT_ACK, read_reg(VIRTIO_MMIO_INTERRUPT_STATUS));


  /* cycle through all the active rings */
  for (int r = 0; r < VIO_MAX_RINGS; r++) {
    auto *ring = &this->ring[r];
    if (!ring->active) continue;

    int cur_idx = ring->used->idx;
    for (int i = ring->last_used; i != (cur_idx & ring->num_mask); i = (i + 1) & ring->num_mask) {
      // process chain
      auto *used_elem = &ring->used->ring[i];

      handle_used(r, used_elem);
      ring->last_used = (ring->last_used + 1) & ring->num_mask;
    }
  }

  // XXX is this safe?
}




class virtio_mmio_dev {
 protected:
  /* Virtual address of the registers */
  volatile uint32_t *regs = NULL;

 public:
  struct vring ring[VIO_MAX_RINGS];


  inline auto &mmio_regs(void) { return *(virtio::mmio_regs *)regs; }
  inline uint32_t read_reg(int off) { return *(volatile uint32_t *)((off_t)regs + off); }
  inline void write_reg(int off, uint32_t val) { *(volatile uint32_t *)((off_t)regs + off) = val; }
  virtio_mmio_dev(volatile uint32_t *regs);
  virtual ~virtio_mmio_dev(void);

  virtual bool initialize(const struct virtio_config &config) { return false; }

  void register_virtio_irq(int irq);
  void dispatch_virtio_irq();
  virtual void virtio_irq(int ring_index, virtio::virtq_used_elem *) {}

  int alloc_ring(int index, int len);

  // find a free descriptor, mark it non-free, return its index.
  uint16_t alloc_desc(int ring_index);

  // mark a descriptor as free.
  void free_desc(int ring_index, int i);
  void submit_chain(int ring_index, int desc_index);

  virtio::virtq_desc *alloc_desc_chain(int ring_index, int count, uint16_t *start_index);
  inline virtio::virtq_desc *index_to_desc(int ring_index, int index) {
    auto d = &ring[ring_index].desc[index];
    return d;
  }

  inline void kick(int ring) {
    __sync_synchronize();
    write_reg(VIRTIO_MMIO_QUEUE_NOTIFY, ring);
    __sync_synchronize();
  }
};


#endif
