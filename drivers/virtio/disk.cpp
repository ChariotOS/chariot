#include <dev/virtio/mmio.h>
#include "dev/driver.h"
#include "internal.h"
#include <sched.h>


// #define DO_LOG

#ifdef DO_LOG
#define LOG(...) PFXLOG(YEL "VIRTIO-BLK", __VA_ARGS__)
#else
#define LOG(...)
#endif

DECLARE_STUB_DRIVER("virtio-mmio-disk", virtio_mmio_disk_driver)

VirtioMMIODisk::VirtioMMIODisk(virtio_config &cfg) : VirtioMMIO<dev::Disk>(cfg, virtio_mmio_disk_driver) {}

bool VirtioMMIODisk::initialize(void) {
  LOG("initialize\n");
  set_block_size(diskconfig().blk_size);
  set_block_count(diskconfig().capacity);
  set_size(block_size() * block_count());

  uint32_t status = 0;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  write_reg(VIRTIO_MMIO_STATUS, status);

  status |= VIRTIO_CONFIG_S_DRIVER;
  write_reg(VIRTIO_MMIO_STATUS, status);

  // negotiate features
  uint64_t features = read_reg(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  write_reg(VIRTIO_MMIO_DRIVER_FEATURES, features);
  register_virtio_irq(config.irqnr);

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  write_reg(VIRTIO_MMIO_STATUS, status);

  int ndesc = 64;

  alloc_ring(0, ndesc);
  ops = new virtio::blk_req[ndesc];

  dev::register_disk(this);



  return true;
}


int VirtioMMIODisk::read_blocks(uint32_t sector, void *data, int nsec) {
  LOG("read_blocks %d %p %d\n", sector, data, nsec);
  disk_rw(sector, data, nsec, 0 /* read mode */);
  return true;
}

int VirtioMMIODisk::write_blocks(uint32_t sector, const void *data, int nsec) {
  LOG("write_blocks %d %p %d\n", sector, data, nsec);
  disk_rw(sector, (uint8_t *)data, nsec, 1 /* read mode */);
  return true;
}


struct virtio_blk_req {
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
  /* NO DATA INCLUDED HERE! */
  uint8_t status;
} __attribute__((packed));


int VirtioMMIODisk::disk_rw(uint32_t sector, void *data, int n, int write) {
  size_t size = n * block_size();


  vdisk_lock.lock();

  {
    assert(size == 512);
    struct virtio_blk_req req;
    req.sector = sector;
		req.reserved = 0;
    req.type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    req.status = 0xFF;

    uint16_t first_index;
    alloc_desc_chain(0, 3, &first_index);

    ck::vec<VirtioMMIOVring::Descriptor> descs;

    descs.push({
        .addr = (uint64_t)v2p(&req),
        .len = sizeof(req),
        .flags = VRING_DESC_F_NEXT,
    });

    descs.push({
        .addr = (uint64_t)v2p(data),
        .len = (uint32_t)size,
        .flags = (uint16_t)((write ? 0 : VRING_DESC_F_WRITE) | VRING_DESC_F_NEXT),
    });

    descs.push({
        .addr = (uint64_t)v2p(&req.status),
        .len = 1,
        .flags = VRING_DESC_F_WRITE,
    });

    submit(descs, first_index);

    while (req.status == 0xFF) {
      arch_relax();
    }

    free_desc_chain(0, first_index);
  }

  vdisk_lock.unlock();

  return true;
}


void VirtioMMIODisk::virtio_irq(int ring_index, virtio::virtq_used_elem *e) {
  LOG("dev %p, ring %u, e %p, id %u, len %u\n", this, ring_index, e, e->id, e->len);

  /* parse our descriptor chain, add back to the free queue */
  uint16_t i = e->id;
  for (;;) {
    int next;
    auto *desc = index_to_desc(ring_index, i);

    // virtio_dump_desc(desc);

    if (desc->flags & VRING_DESC_F_NEXT) {
      next = desc->next;
    } else {
      /* end of chain */
      next = -1;
    }

    free_desc(ring_index, i);

    if (next < 0) break;
    i = next;
  }
}
