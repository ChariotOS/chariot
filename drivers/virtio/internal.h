#pragma once

#include <dev/disk.h>
#include <dev/virtio_mmio.h>

#include <errno.h>
#include <phys.h>
#include <printk.h>
#include <time.h>
#include <util.h>


#define VIO_PGCOUNT 2
#define VIO_NUM_DESC 8

class virtio_mmio_dev {
 protected:
	 /* Virtual address of the registers */
  volatile uint32_t *regs = NULL;
  /* virtio communicates mostly through RAM, so this is the pointer to those pages */
  void *pages = NULL;

  // pages[] is divided into three regions (descriptors, avail, and
  // used), as explained in Section 2.6 of the virtio specification
  // for the legacy interface.
  // https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf

  // the first region of pages[] is a set (not a ring) of DMA
  // descriptors, with which the driver tells the device where to read
  // and write individual disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  // points into pages[].
  struct virtio::virtq_desc *desc;

  // next is a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  // points into pages[].
  struct virtio::virtq_avail *avail;

  // finally a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  // points into pages[].
  struct virtio::virtq_used *used;


  bool free[VIO_NUM_DESC]; /* Is a descriptor free? */
  uint16_t used_idx;       // we've looked this far in used[2..NUM].

  spinlock vdisk_lock;

 public:
  inline volatile uint32_t read_reg(int off) { return *(volatile uint32_t *)((off_t)regs + off); }
  inline void write_reg(int off, uint32_t val) { *(volatile uint32_t *)((off_t)regs + off) = val; }
  virtio_mmio_dev(volatile uint32_t *regs);
  ~virtio_mmio_dev(void);

  virtual void irq(void) {}

  // find a free descriptor, mark it non-free, return its index.
  int alloc_desc();

  // mark a descriptor as free.
  void free_desc(int i);

  // free a chain of descriptors.
  void free_chain(int i);

  // allocate three descriptors (they need not be contiguous).
  // disk transfers always use three descriptors.
  int alloc3_desc(int *idx);
};


class virtio_mmio_disk : public virtio_mmio_dev, public dev::disk {
  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    void *data;
    char status;
    wait_queue wq;
  } info[VIO_NUM_DESC];

  // disk command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio::blk_req ops[VIO_NUM_DESC];
  spinlock vdisk_lock;

 public:
  virtio_mmio_disk(volatile uint32_t *regs);

  void disk_rw(uint32_t sector, void *data, int n, int write);

  virtual size_t block_size();
  virtual size_t block_count();
  virtual bool read_blocks(uint32_t sector, void *data, int nsec = 1);
  virtual bool write_blocks(uint32_t sector, const void *data, int nsec = 1);

  virtual void irq(void);

  inline auto &config(void) { return *(virtio::blk_config *)((off_t)this->regs + 0x100); }
};
