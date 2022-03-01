#pragma once

#include <lock.h>
#include <wait.h>
#include <dev/disk.h>
#include <dev/virtio/mmio.h>


class virtio_mmio_disk : public virtio_mmio_dev, public dev::Disk {
  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    void *data;
    uint8_t status;
    wait_queue wq;
  } info[VIO_NUM_DESC];

  // disk command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio::blk_req *ops = NULL;
  spinlock vdisk_lock;

 public:
  virtio_mmio_disk(volatile uint32_t *regs);
  void irq(int ring_index, virtio::virtq_used_elem *) override;

  void disk_rw(uint32_t sector, void *data, int n, int write);

  int read_blocks(uint32_t sector, void *data, int nsec = 1) override;
  int write_blocks(uint32_t sector, const void *data, int nsec = 1) override;

  bool initialize(const struct virtio_config &config) override;

  inline auto &config(void) { return *(virtio::blk_config *)((off_t)this->regs + 0x100); }
};
