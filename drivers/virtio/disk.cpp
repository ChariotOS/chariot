#include <dev/virtio_mmio.h>
#include "internal.h"


virtio_mmio_disk::virtio_mmio_disk(volatile uint32_t *regs) : virtio_mmio_dev(regs) {
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

  irq::install(1, virtio_irq_handler, "virtio disk", (void *)this);

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  write_reg(VIRTIO_MMIO_STATUS, status);

	int ndesc = 64;

  alloc_ring(0, ndesc);
	ops = new virtio::blk_req[ndesc];
}

void virtio_mmio_disk::disk_rw(uint32_t sector, void *data, int n, int write) {
  /*
   * God knows where `data` points to. It could be a virtual address that can't be
   * easily converted to a physical one. For this reason, we need to allocate one
   * that we know is physically contiguous. Just use malloc for this, as it's physically
   * contiguous (for now) TODO: be smart later :^)
   */
  void *tmp_buf = malloc(block_size());
	
  if (write) memcpy(tmp_buf, data, block_size());

	if (!arch_irqs_enabled()) {
		panic("wut\n");
	}

  vdisk_lock.lock();


  // the spec's Section 5.2 says that legacy block operations use
  // three descriptors: one for type/reserved/sector, one for the
  // data, one for a 1-byte status result.

  // allocate the three descriptors.

  uint16_t first_index;


  virtio::virtq_desc *start = NULL;

  start = alloc_desc_chain(0, 3, &first_index);
  if (start == NULL) {
    panic("start is null!\n");
  }


  virtio::virtq_desc *desc[3];
  desc[0] = start;
  desc[1] = index_to_desc(0, desc[0]->next);
  desc[2] = index_to_desc(0, desc[1]->next);
	assert(desc[2] != NULL);


  struct virtio::blk_req *buf0 = &ops[first_index];

  if (write)
    buf0->type = VIRTIO_BLK_T_OUT;  // write the disk
  else
    buf0->type = VIRTIO_BLK_T_IN;  // read the disk
  buf0->reserved = 0;
  buf0->sector = sector;
  desc[0]->addr = (uint64_t)v2p(buf0);
  desc[0]->len = sizeof(virtio::blk_req);
  desc[0]->flags = VRING_DESC_F_NEXT;

  desc[1]->addr = (uint64_t)v2p(tmp_buf);
  desc[1]->len = 512 * n;
  if (write)
    desc[1]->flags = 0;  // device reads b->data
  else
    desc[1]->flags = VRING_DESC_F_WRITE;  // device writes b->data
  desc[1]->flags |= VRING_DESC_F_NEXT;

  info[first_index].status = 0xff;  // device writes 0 on success
  desc[2]->addr = (uint64_t)v2p(&info[first_index].status);
  desc[2]->len = 1;
  desc[2]->flags = VRING_DESC_F_WRITE;  // device writes the status
  desc[2]->next = 0;

  submit_chain(0, first_index);

  __sync_synchronize();

// #define VIRTIO_BLK_WAITQUEUE
#ifdef VIRTIO_BLK_WAITQUEUE

  struct wait_entry ent;
  ent.flags = 0;
  ent.wq = &info[first_index].wq;

  ent.wq->lock.lock();

  sched::set_state(PS_UNINTERRUPTIBLE);
  ent.wq->task_list.add(&ent.item);

  ent.wq->lock.unlock();

#endif


  write_reg(VIRTIO_MMIO_QUEUE_NOTIFY, 0);

  __sync_synchronize();

#ifdef VIRTIO_BLK_WAITQUEUE
  sched::yield();
#endif

  int loops = 0;
  while (info[first_index].status == 0xff) {
    __sync_synchronize();
    loops++;
  }  // device writes 0 on success

  info[first_index].data = NULL;

  vdisk_lock.unlock();

  if (!write) {
    memcpy(data, tmp_buf, block_size());
    // printk("block %d\n", sector);
    // hexdump(data, block_size(), true);
    // printk("\n\n");
  }

  ::free(tmp_buf);

  return;
}


void virtio_mmio_disk::irq(int ring_index, virtio::virtq_used_elem *e) {
  // printk("dev %p, ring %u, e %p, id %u, len %u\n", this, ring_index, e, e->id, e->len);

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



bool virtio_mmio_disk::read_blocks(uint32_t sector, void *data, int nsec) {
  disk_rw(sector, data, nsec, 0 /* read mode */);
  return true;
}

bool virtio_mmio_disk::write_blocks(uint32_t sector, const void *data, int nsec) {
  disk_rw(sector, (uint8_t *)data, nsec, 1 /* read mode */);
  return true;
}

size_t virtio_mmio_disk::block_size(void) {
  return config().blk_size;
}

size_t virtio_mmio_disk::block_count(void) {
  return config().capacity;
}

