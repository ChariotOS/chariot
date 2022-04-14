#pragma once

#include <lock.h>
#include <wait.h>
#include <dev/disk.h>
#include <dev/virtio/mmio.h>


class VirtioMMIODisk : public VirtioMMIO<dev::Disk> {
 private:
  spinlock vdisk_lock;
	wait_queue wq;

 public:
  VirtioMMIODisk(virtio_config &cfg);
  virtual ~VirtioMMIODisk() {}

  // ^VirtioMMIO::
  bool initialize(void) override;
  void handle_used(int ring_index, virtio::virtq_used_elem *) override;

  // ^dev::BlockDevice::
  int read_blocks(uint32_t sector, void *data, int nsec) override;
  int write_blocks(uint32_t sector, const void *data, int nsec) override;



 protected:
  int disk_rw(uint32_t sector, void *data, int n, int write);
  inline auto &diskconfig(void) { return *(virtio::blk_config *)((off_t)this->regs + 0x100); }
};
