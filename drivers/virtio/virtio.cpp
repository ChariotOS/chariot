#include <dev/virtio_mmio.h>
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


virtio_mmio_dev::virtio_mmio_dev(volatile uint32_t *regs) : regs(regs) {
  /* allocate 2 pages */
  pages = phys::alloc(VIO_PGCOUNT);

  /* Tell the device what the page size is */
  write_reg(VIRTIO_MMIO_GUEST_PAGE_SIZE, PGSIZE);


  // initialize queue 0.
  write_reg(VIRTIO_MMIO_QUEUE_SEL, 0);
  uint32_t max = read_reg(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max == 0) panic("virtio disk has no queue 0");
  if (max < VIO_NUM_DESC) panic("virtio disk max queue too short");
  write_reg(VIRTIO_MMIO_QUEUE_NUM, VIO_NUM_DESC);
  write_reg(VIRTIO_MMIO_QUEUE_PFN, ((off_t)pages) >> 12);

  // desc = pages -- num * virtq_desc
  // avail = pages + 0x40 -- 2 * uint16, then num * uint16
  // used = pages + 4096 -- 2 * uint16, then num * vRingUsedElem
  desc = (struct virtio::virtq_desc *)pages;
  avail = (struct virtio::virtq_avail *)((off_t)pages + VIO_NUM_DESC * sizeof(virtio::virtq_desc));
  used = (struct virtio::virtq_used *)((off_t)pages + PGSIZE);

  // all NUM descriptors start out unused.
  for (int i = 0; i < VIO_NUM_DESC; i++) free[i] = 1;
}

virtio_mmio_dev::~virtio_mmio_dev(void) { phys::free(pages, VIO_PGCOUNT); }



int virtio_mmio_dev::alloc_desc() {
  for (int i = 0; i < VIO_NUM_DESC; i++) {
    if (free[i]) {
      free[i] = 0;
      return i;
    }
  }
  return -1;
}

inline void virtio_mmio_dev::free_desc(int i) {
  if (i >= VIO_NUM_DESC) panic("free_desc 1");
  if (free[i]) panic("free_desc 2");
  desc[i].addr = 0;
  desc[i].len = 0;
  desc[i].flags = 0;
  desc[i].next = 0;
  free[i] = 1;
  // wakeup(&free[0]);
}


void virtio_mmio_dev::free_chain(int i) {
  while (1) {
    int flag = desc[i].flags;
    int nxt = desc[i].next;
    free_desc(i);
    if (flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}


int virtio_mmio_dev::alloc3_desc(int *idx) {
  for (int i = 0; i < 3; i++) {
    idx[i] = alloc_desc();
    if (idx[i] < 0) {
      for (int j = 0; j < i; j++) free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}



static void virtio_irq_handler(int i, reg_t *r, void *data) {
  auto *dev = (virtio_mmio_dev *)data;
  dev->irq();
}




virtio_mmio_disk::virtio_mmio_disk(volatile uint32_t *regs) : virtio_mmio_dev(regs) {
  uint32_t status = 0;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  write_reg(VIRTIO_MMIO_STATUS, status);

  status |= VIRTIO_CONFIG_S_DRIVER;
  write_reg(VIRTIO_MMIO_STATUS, status);

  char vendor[5];
  uint32_t vendor_id = read_reg(VIRTIO_MMIO_VENDOR_ID);
  memcpy(vendor, &vendor_id, 4);
  vendor[4] = 0;
  printk(KERN_INFO " - Vendor: %s\n", vendor);

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

}

void virtio_mmio_disk::disk_rw(uint32_t sector, void *data, int n, int write) {
  vdisk_lock.lock();
	
  // the spec's Section 5.2 says that legacy block operations use
  // three descriptors: one for type/reserved/sector, one for the
  // data, one for a 1-byte status result.

  // allocate the three descriptors.
  int idx[3];
  while (1) {
    if (alloc3_desc(idx) == 0) {
      break;
    }
    panic("oof\n");
    // sleep(&free[0], &vdisk_lock);
  }

  // format the three descriptors.
  // qemu's virtio-blk.c reads them.

  struct virtio::blk_req *buf0 = &ops[idx[0]];

  if (write)
    buf0->type = VIRTIO_BLK_T_OUT;  // write the disk
  else
    buf0->type = VIRTIO_BLK_T_IN;  // read the disk
  buf0->reserved = 0;
  buf0->sector = sector;

  desc[idx[0]].addr = (uint64_t)buf0;
  desc[idx[0]].len = sizeof(virtio::blk_req);
  desc[idx[0]].flags = VRING_DESC_F_NEXT;
  desc[idx[0]].next = idx[1];

  desc[idx[1]].addr = (uint64_t)data;
  desc[idx[1]].len = 512 * n;
  if (write)
    desc[idx[1]].flags = 0;  // device reads b->data
  else
    desc[idx[1]].flags = VRING_DESC_F_WRITE;  // device writes b->data
  desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  desc[idx[1]].next = idx[2];

  info[idx[0]].status = 0xff;  // device writes 0 on success
  desc[idx[2]].addr = (uint64_t)&info[idx[0]].status;
  desc[idx[2]].len = 1;
  desc[idx[2]].flags = VRING_DESC_F_WRITE;  // device writes the status
  desc[idx[2]].next = 0;

  // record struct buf for virtio_disk_intr().
  info[idx[0]].data = data;

  // tell the device the first index in our chain of descriptors.
  avail->ring[avail->idx % VIO_NUM_DESC] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.
  avail->idx += 1;  // not % NUM ...

  __sync_synchronize();


  struct wait_entry ent;
  ent.flags = 0;
  ent.wq = &info[idx[0]].wq;

  ent.wq->lock.lock();

  sched::set_state(PS_UNINTERRUPTIBLE);
  ent.wq->task_list.add(&ent.item);

  ent.wq->lock.unlock();

  write_reg(VIRTIO_MMIO_QUEUE_NOTIFY, 0);  // value is queue number

  sched::yield();

  info[idx[0]].data = NULL;
  free_chain(idx[0]);

  vdisk_lock.unlock();

  return;
}



void virtio_mmio_disk::irq(void) {
  // the device won't raise another interrupt until we tell it
  // we've seen this interrupt, which the following line does.
  // this may race with the device writing new entries to
  // the "used" ring, in which case we may process the new
  // completion entries in this interrupt, and have nothing to do
  // in the next interrupt, which is harmless.
  write_reg(VIRTIO_MMIO_INTERRUPT_ACK, read_reg(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3);

  __sync_synchronize();

  // the device increments disk.used->idx when it
  // adds an entry to the used ring.
  while (used_idx != used->idx) {
    __sync_synchronize();
    int id = used->ring[used_idx % VIO_NUM_DESC].id;

    if (info[id].status != 0) panic("virtio_disk_intr status");

    auto *b = &info[id];
    b->wq.wake_up_all();
    __sync_synchronize();

    used_idx += 1;
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

#define REG(off) ((volatile uint32_t *)((off_t)regs + off))


int virtio_check_mmio_disk(volatile uint32_t *regs) {
  printk("\n\n");
  printk(KERN_INFO "Virtio disk at %p\n", regs);
  auto *disk = new virtio_mmio_disk(regs);

	dev::register_disk(disk);

  printk("\n\n");
  return 0;
}


int virtio::check_mmio(void *addr) {
  auto *regs = (volatile uint32_t *)addr;

  uint32_t magic = *REG(VIRTIO_MMIO_MAGIC_VALUE);
  if (memcmp(&magic, "virt", 4) != 0) {
    printk(KERN_WARN "Wrong magic value 0x%08lx!\n", magic);
    return -ENODEV;
  }


  uint32_t dev_id = *REG(VIRTIO_MMIO_DEVICE_ID);
  switch (dev_id) {
    /* virtio disk */
    case 2:
      return virtio_check_mmio_disk(regs);
    default:
      // printk("No handler for device id %d\n", dev_id);
      return -ENODEV;
  }


  printk("\n");
  return 0;
}
