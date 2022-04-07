#include <dev/virtio/mmio.h>
#include <module.h>
#ifdef X86
#include <pci.h>
#endif
#include <dev/mbr.h>
#include <errno.h>
#include <phys.h>
#include <printf.h>
#include <time.h>
#include <util.h>

#include "internal.h"

// #define DO_LOG

#ifdef DO_LOG
#define VIRTIO_DEBUG(...) PFXLOG(GRN "VIRTIO", __VA_ARGS__)
#define VRING_LOG(...) PFXLOG(MAG "VRING", __VA_ARGS__)
#else
#define VIRTIO_DEBUG(...)
#define VRING_LOG(...)
#endif

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
    }
  });
}

module_init("virtio", virtio_pci_init);
#endif




#define REG(off) ((volatile uint32_t *)((off_t)regs + off))

virtio_mmio_dev::virtio_mmio_dev(volatile uint32_t *regs) : regs((uint32_t *)p2v(regs)) {}
virtio_mmio_dev::~virtio_mmio_dev(void) {}
int virtio_mmio_dev::alloc_ring(int index, int len) { return 0; }
uint16_t virtio_mmio_dev::alloc_desc(int ring_index) { return 0; }
void virtio_mmio_dev::submit_chain(int ring_index, int desc_index) {}
virtio::virtq_desc *virtio_mmio_dev::alloc_desc_chain(int ring_index, int count, uint16_t *start_index) { return NULL; }
void virtio_mmio_dev::free_desc(int ring_index, int desc_index) {}
void virtio_mmio_dev::dispatch_virtio_irq() {}
void virtio_irq_handler(int i, reg_t *r, void *data) {}
void virtio_mmio_dev::register_virtio_irq(int irq) {}




//////////////////////////////////////////////////////////////////////////



int VirtioMMIOVring::alloc_ring(int index, int len) {
  VRING_LOG("alloc_ring - index:%d, len:%d\n", index, len);
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


uint16_t VirtioMMIOVring::alloc_desc(int ring_index) {
  VRING_LOG("alloc_desc - ring_index:%d\n", ring_index);
  if (ring[ring_index].free_count == 0) return 0xffff;
  assert(ring[ring_index].free_list != 0xffff);

  uint16_t i = ring[ring_index].free_list;
  auto *desc = &ring[ring_index].desc[i];
  ring[ring_index].free_list = desc->next;
  ring[ring_index].free_count--;

  return i;
}



void VirtioMMIOVring::free_desc(int ring_index, int desc_index) {
  // VRING_LOG("free_desc - ring_index:%d, desc_index:%d\n", ring_index, desc_index);
  // printf("ring %u index %u free_count %u\n", ring_index, desc_index,
  // ring[ring_index].free_count);
  ring[ring_index].desc[desc_index].next = ring[ring_index].free_list;
  ring[ring_index].free_list = desc_index;
  ring[ring_index].free_count++;
}


void VirtioMMIOVring::free_desc_chain(int ring_index, int desc_index) {

	auto &desc = ring[ring_index].desc[desc_index];
	int next = desc.next;

	free_desc(ring_index, desc_index);

	if (next != 0) {
		free_desc_chain(ring_index, next);
	}
}


void VirtioMMIOVring::submit_chain(int ring_index, int desc_index) {
  VRING_LOG("submit_chain - ring_index:%d, desc_index:%d\n", ring_index, desc_index);
  /* add the chain to the available list */
  auto *avail = ring[ring_index].avail;

  avail->ring[avail->idx & ring[ring_index].num_mask] = desc_index;
  __sync_synchronize();
  // mb();
  avail->idx += 1;
  __sync_synchronize();
	kick(ring_index);
}


virtio::virtq_desc *VirtioMMIOVring::alloc_desc_chain(int ring_index, int count, uint16_t *start_index) {
  VRING_LOG("alloc_desc_chain - ring_index:%d, count:%d\n", ring_index, count);
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


int VirtioMMIOVring::submit(const ck::vec<VirtioMMIOVring::Descriptor> &descs, uint16_t first_index) {
  // SPOOKY: vla.
  virtio::virtq_desc *hwdescs[descs.size()];

  hwdescs[0] = index_to_desc(0, first_index);
  for_range(i, 1, descs.size()) {
    // build out the descriptor chain
    hwdescs[i] = index_to_desc(0, hwdescs[i - 1]->next);
  }



	// printf("start = %d\n", first_index);
  for_range(i, 0, descs.size()) {
    hwdescs[i]->addr = descs[i].addr;
    hwdescs[i]->len = descs[i].len;
    hwdescs[i]->flags = descs[i].flags;

    // printf("vring desc[%d] = addr: %p, len: %d, flags: %04x, next: %d\n", i, hwdescs[i]->addr, hwdescs[i]->len, hwdescs[i]->flags, hwdescs[i]->next);

    if (i != descs.size() - 1) {
      hwdescs[i]->flags |= VRING_DESC_F_NEXT;
    } else {
			hwdescs[i]->next = 0;
		}
  }

  submit_chain(0, first_index);

  __sync_synchronize();

  // success
  return 0;
}

//////////////////////////////////////////////////////////////////////////




class VirtioMMIODriver : public dev::Driver {
 public:
  VirtioMMIODriver() { set_name("virtio-mmio"); }

  dev::ProbeResult probe(ck::ref<hw::Device> dev) override;
};

static ck::ref<VirtioMMIODriver> virtio_driver = nullptr;


int virtio::check_mmio(void *addr, int irq) {
  auto *regs = (volatile uint32_t *)p2v(addr);

  uint32_t magic = *REG(VIRTIO_MMIO_MAGIC_VALUE);
  if (memcmp(&magic, "virt", 4) != 0) {
    printf(KERN_WARN "Wrong magic value 0x%08lx!\n", magic);
    return -ENODEV;
  }

  struct virtio_config config;
  config.irqnr = irq;
  config.regs = regs;


  uint32_t dev_id = *REG(VIRTIO_MMIO_DEVICE_ID);

  // Virtio Block
  if (dev_id == 2) {
    VIRTIO_DEBUG("Disk Device at %p with irq %d\n", addr, irq);
    auto dev = ck::make_ref<VirtioMMIODisk>(config);
    // Initialize the device
    if (dev->initialize()) {
      VIRTIO_DEBUG("Disk device initialized\n");
    } else {
      VIRTIO_DEBUG("Disk device failed to initialize\n");
    }
  }

  return -ENODEV;

  switch (dev_id) {
    case 0:
      return -ENODEV;
    /* virtio disk */
    case 2: {
      break;
    }


    case 16: {
      VIRTIO_DEBUG("GPU Device at %p with irq %d\n", addr, irq);
      break;
    }

    case 18: {
      VIRTIO_DEBUG("[VIRTIO] Input Device at %p with irq %d\n", addr, irq);
      break;
    }
    default:
      VIRTIO_DEBUG("No handler for device id %d at %p\n", dev_id, addr);
      return -ENODEV;
  }

  /*
if (dev == NULL) return -ENODEV;

if (!dev->initialize(config)) {
delete dev;
printf(KERN_ERROR "virtio device at %p failed to initialize!\n", regs);
return -ENODEV;
}
  */

  return 0;
}


dev::ProbeResult VirtioMMIODriver::probe(ck::ref<hw::Device> dev) {
  if (auto mmio = dev->cast<hw::MMIODevice>()) {
    if (mmio->is_compat("virtio,mmio")) {
      if (virtio::check_mmio((void *)mmio->address(), mmio->interrupt) >= 0) {
        return dev::ProbeResult::Attach;
      }
    }
  }
  return dev::ProbeResult::Ignore;
}

static void virtio_init(void) {
  virtio_driver = ck::make_ref<VirtioMMIODriver>();
  dev::Driver::add(virtio_driver);
}

module_init("virtio-mmio", virtio_init);
