#pragma once

#include <dev/virtio/mmio.h>


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
  struct virtio::blk_req *ops = NULL;
  spinlock vdisk_lock;

 public:
  virtio_mmio_disk(volatile uint32_t *regs);
  virtual void irq(int ring_index, virtio::virtq_used_elem *);

  void disk_rw(uint32_t sector, void *data, int n, int write);

  virtual size_t block_size();
  virtual size_t block_count();
  virtual bool read_blocks(uint32_t sector, void *data, int nsec = 1);
  virtual bool write_blocks(uint32_t sector, const void *data, int nsec = 1);

  virtual bool initialize(const struct virtio_config &config);


  inline auto &config(void) {
    return *(virtio::blk_config *)((off_t)this->regs + 0x100);
  }
};

