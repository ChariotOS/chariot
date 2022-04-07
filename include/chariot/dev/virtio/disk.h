#pragma once

#include <lock.h>
#include <wait.h>
#include <dev/disk.h>
#include <dev/virtio/mmio.h>


class VirtioMMIODisk : public VirtioMMIO<dev::Disk> {
 private:
  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    uint8_t status;
    void *data;
    wait_queue wq;
  } info[VIO_NUM_DESC];

  // disk command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio::blk_req *ops = NULL;
  spinlock vdisk_lock;

 public:
  VirtioMMIODisk(virtio_config &cfg);
  virtual ~VirtioMMIODisk() {}

  // ^VirtioMMIO::
  bool initialize(void) override;
  void virtio_irq(int ring_index, virtio::virtq_used_elem *) override;

  // ^dev::BlockDevice::
  int read_blocks(uint32_t sector, void *data, int nsec) override;
  int write_blocks(uint32_t sector, const void *data, int nsec) override;



 protected:
  int disk_rw(uint32_t sector, void *data, int n, int write);
  inline auto &diskconfig(void) { return *(virtio::blk_config *)((off_t)this->regs + 0x100); }
};
