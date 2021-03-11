#include <dev/virtio/mmio.h>
#include <module.h>
#ifdef X86
#include <pci.h>
#endif
#include <dev/mbr.h>
#include <errno.h>
#include <phys.h>
#include <printk.h>
#include <time.h>
#include <util.h>

#include "internal.h"

#ifdef X86
void virtio_pci_init(void) {
  return;
  pci::walk_devices([](pci::device *d) {
    //
    if (d->vendor_id == 0x1af4) {
      auto &cfg = d->cfg;
      hexdump(&cfg, sizeof(cfg), true);

      debug("Virtio Device Found. Subsys: %04x.\n", cfg.dev_cfg.subsys_id);
      switch (cfg.dev_cfg.subsys_id) {
        case 1:
          debug("Net Device\n");
          // vdev->type = VIRTIO_PCI_NET;
          break;
        case 2:
          debug("Block Device\n");
          // vdev->type = VIRTIO_PCI_BLOCK;
          break;
        case 3:
          debug("Console Device\n");
          // vdev->type = VIRTIO_PCI_CONSOLE;
          break;
        case 4:
          debug("Entropy Device\n");
          // vdev->type = VIRTIO_PCI_ENTROPY;
          break;
        case 5:
          debug("Balloon Device\n");
          // vdev->type = VIRTIO_PCI_BALLOON;
          break;
        case 6:
          debug("IOMemory Device\n");
          // vdev->type = VIRTIO_PCI_IOMEM;
          break;
        case 7:
          debug("rpmsg Device\n");
          // vdev->type = VIRTIO_PCI_RPMSG;
          break;
        case 8:
          debug("SCSI Host Device\n");
          // vdev->type = VIRTIO_PCI_SCSI_HOST;
          break;
        case 9:
          debug("9P Transport Device\n");
          // vdev->type = VIRTIO_PCI_9P;
          break;
        case 10:
          debug("WIFI Device\n");
          // vdev->type = VIRTIO_PCI_WIFI;
          break;
        case 11:
          debug("rproc serial Device\n");
          // vdev->type = VIRTIO_PCI_RPROC_SERIAL;
          break;
        case 12:
          debug("CAIF Device\n");
          // vdev->type = VIRTIO_PCI_CAIF;
          break;
        case 13:
          debug("Fancier Balloon Device\n");
          // vdev->type = VIRTIO_PCI_FANCIER_BALLOON;
          break;
        case 16:
          debug("GPU Device\n");
          // vdev->type = VIRTIO_PCI_GPU;
          break;
        case 17:
          debug("Timer Device\n");
          // vdev->type = VIRTIO_PCI_TIMER;
          break;
        case 18:
          debug("Input Device\n");
          // vdev->type = VIRTIO_PCI_INPUT;
          break;
        default:
          debug("Unknown Device\n");
          // vdev->type = VIRTIO_PCI_UNKNOWN;
          break;
      }



      // struct virtio_pci_dev *vdev;
    }
  });
}

module_init("virtio", virtio_pci_init);
#endif


virtio_mmio_dev::virtio_mmio_dev(volatile uint32_t *regs) : regs((uint32_t *)p2v(regs)) {
  /* Leave initialization up to the subclass */
}

virtio_mmio_dev::~virtio_mmio_dev(void) {
}


int virtio_mmio_dev::alloc_ring(int index, int len) {
  /* Some logical asserts */
  assert(index >= 0 && index < VIO_MAX_RINGS);
  assert(ring[index].active == false);

  struct vring *ring = &this->ring[index];

  size_t size = vring_size(len, PGSIZE);
  int npages = round_up(size, 4096) >> 12;

  off_t pa = (off_t)phys::alloc(npages);
  void *vptr = p2v(pa);

  /* initialize the ring */
  vring_init(ring, len, vptr, PGSIZE);
  ring[index].free_list = 0xffff;
  ring[index].free_count = 0;

  /* add all the descriptors to the free list */
  for (int i = 0; i < len; i++)
    free_desc(index, i);

  /* Select the index of this ring */
  write_reg(VIRTIO_MMIO_QUEUE_SEL, index);
  write_reg(VIRTIO_MMIO_GUEST_PAGE_SIZE, PGSIZE);
  write_reg(VIRTIO_MMIO_QUEUE_NUM, len);
  write_reg(VIRTIO_MMIO_QUEUE_ALIGN, 4096);
  write_reg(VIRTIO_MMIO_QUEUE_PFN, pa / PGSIZE);

  ring->active = true;
  return 0;
}




uint16_t virtio_mmio_dev::alloc_desc(int ring_index) {
  if (ring[ring_index].free_count == 0) return 0xffff;
  assert(ring[ring_index].free_list != 0xffff);

  uint16_t i = ring[ring_index].free_list;
  auto *desc = &ring[ring_index].desc[i];
  ring[ring_index].free_list = desc->next;
  ring[ring_index].free_count--;

  return i;
}


void virtio_mmio_dev::submit_chain(int ring_index, int desc_index) {
  /* add the chain to the available list */
  auto *avail = ring[ring_index].avail;

  avail->ring[avail->idx & ring[ring_index].num_mask] = desc_index;
  __sync_synchronize();
  // mb();
  avail->idx++;
}


void virtio_mmio_dev::free_desc(int ring_index, int desc_index) {
  // printk("ring %u index %u free_count %u\n", ring_index, desc_index,
  // ring[ring_index].free_count);
  ring[ring_index].desc[desc_index].next = ring[ring_index].free_list;
  ring[ring_index].free_list = desc_index;
  ring[ring_index].free_count++;
}




void virtio_mmio_dev::irq(void) {
  // the device won't raise another interrupt until we tell it
  // we've seen this interrupt, which the following line does.
  // this may race with the device writing new entries to
  // the "used" ring, in which case we may process the new
  // completion entries in this interrupt, and have nothing to do
  // in the next interrupt, which is harmless.

  int irq_status = read_reg(VIRTIO_MMIO_INTERRUPT_STATUS);

  if (irq_status & 0x1) { /* used ring update */
    // XXX is this safe?
    write_reg(VIRTIO_MMIO_INTERRUPT_ACK, 0x1);

    /* cycle through all the active rings */
    for (int r = 0; r < VIO_MAX_RINGS; r++) {
      auto *ring = &this->ring[r];
      if (!ring->active) continue;


      int cur_idx = ring->used->idx;
      for (int i = ring->last_used; i != (cur_idx & ring->num_mask); i = (i + 1) & ring->num_mask) {
        // process chain
        auto *used_elem = &ring->used->ring[i];
        // LTRACEF("id %u, len %u\n", used_elem->id, used_elem->len);

        // DEBUG_ASSERT(dev->irq_driver_callback);
        this->irq(r, used_elem);
        ring->last_used = (ring->last_used + 1) & ring->num_mask;
      }
    }
  }
  if (irq_status & 0x2) { /* config change */
    write_reg(VIRTIO_MMIO_INTERRUPT_ACK, 0x2);
    panic("virtio config change!\n");
  }
}



void virtio_irq_handler(int i, reg_t *r, void *data) {
  auto *dev = (virtio_mmio_dev *)data;
  dev->irq();
}



#define REG(off) ((volatile uint32_t *)((off_t)regs + off))



virtio::virtq_desc *virtio_mmio_dev::alloc_desc_chain(int ring_index, int count,
                                                      uint16_t *start_index) {
  if (ring[ring_index].free_count < count) return NULL;

  /* start popping entries off the chain */
  virtio::virtq_desc *last = 0;
  uint16_t last_index = 0;
  while (count > 0) {
    uint16_t i = ring[ring_index].free_list;
    auto *desc = &ring[ring_index].desc[i];


    ring[ring_index].free_list = desc->next;
    ring[ring_index].free_count--;

    if (last) {
      desc->flags = VRING_DESC_F_NEXT;
      desc->next = last_index;
    } else {
      // first one
      desc->flags = 0;
      desc->next = 0;
    }
    last = desc;
    last_index = i;
    count--;
  }

  if (start_index) *start_index = last_index;

  return last;
}


int virtio::check_mmio(void *addr, int irq) {
  auto *regs = (volatile uint32_t *)p2v(addr);

  uint32_t magic = *REG(VIRTIO_MMIO_MAGIC_VALUE);
  if (memcmp(&magic, "virt", 4) != 0) {
    printk(KERN_WARN "Wrong magic value 0x%08lx!\n", magic);
    return -ENODEV;
  }

  struct virtio_config config;
  config.irqnr = irq;

  virtio_mmio_dev *dev = NULL;


  uint32_t dev_id = *REG(VIRTIO_MMIO_DEVICE_ID);
  switch (dev_id) {
    case 0:
      return -ENODEV;
    /* virtio disk */
    case 2: {
      printk(KERN_INFO "[VIRTIO] Disk Device at %p with irq %d\n", addr, irq);
      dev = new virtio_mmio_disk(regs);
      break;
    }


    case 16: {
      printk("\n\n\n\n\n");
      printk(KERN_INFO "[VIRTIO] GPU Device at %p with irq %d\n", addr, irq);
      /* TODO: do something with this? */
      dev = new virtio_mmio_gpu(regs);
      break;
    }
    default:
      printk("No handler for device id %d at %p\n", dev_id, addr);
      return -ENODEV;
  }

  if (dev == NULL) return -ENODEV;

  if (!dev->initialize(config)) {
    delete dev;
    printk(KERN_ERROR "virtio device at %p failed to initialize!\n", regs);
    return -ENODEV;
  }

  return 0;
}
