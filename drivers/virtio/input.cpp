#include <dev/virtio/input.h>
#include <dev/virtio/mmio.h>
#include <printf.h>
#include "internal.h"

#define BITS_PER_LONG (sizeof(long) * 8)
#define BIT_MASK(nr) (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr) ((nr) / BITS_PER_LONG)
/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static inline void __set_bit(int nr, volatile unsigned long *addr) {
  unsigned long mask = BIT_MASK(nr);
  unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

  *p |= mask;
}

#define EVENTQ 0
#define STATUSQ 1

virtio_mmio_input::virtio_mmio_input(volatile uint32_t *regs) : virtio_mmio_dev(regs) {}
virtio_mmio_input::~virtio_mmio_input(void) {}


bool virtio_mmio_input::initialize(const struct virtio_config &cfg) {
  uint32_t status = 0;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  write_reg(VIRTIO_MMIO_STATUS, status);

  status |= VIRTIO_CONFIG_S_DRIVER;
  write_reg(VIRTIO_MMIO_STATUS, status);


  uint32_t features = read_reg(VIRTIO_MMIO_DEVICE_FEATURES);
  /* TODO: actually negotiate features */
  printf(KERN_INFO "[virtio input] features: %32llb\n", features);
  write_reg(VIRTIO_MMIO_DRIVER_FEATURES, features);
  irq::install(cfg.irqnr, virtio_irq_handler, "virtio input", (void *)this);

  // Device feature negotiation is complete
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  write_reg(VIRTIO_MMIO_STATUS, status);

  /* Allocate the two rings */
  alloc_ring(EVENTQ, 64);  // 64 event descriptors
  alloc_ring(STATUSQ, 8);

  {
    /* Get the name */
    config().subsel = 0;
    config().select = VIRTIO_INPUT_CFG_ID_NAME;
    ck::string s(config().u.string, config().size);
    printf(KERN_INFO "[virtio input] Device Name: \'%s\'\n", s.get());
  }

  {
    config().subsel = 0;
    config().select = VIRTIO_INPUT_CFG_ID_SERIAL;
    ck::string s(config().u.string, config().size);
    printf(KERN_INFO "[virtio input] Device Serial Number: \'%s\'\n", s.get());
  }


  {
    config().subsel = 0;
    config().select = VIRTIO_INPUT_CFG_ID_DEVIDS;

    printf(KERN_INFO "[virtio input] Bus Type: %d\n", config().u.ids.bustype);
    printf(KERN_INFO "[virtio input] Vendor:   %04x\n", config().u.ids.vendor);
    printf(KERN_INFO "[virtio input] Product:  %d\n", config().u.ids.product);
    printf(KERN_INFO "[virtio input] Version:  %d\n", config().u.ids.version);
  }



  for (int i = 0; i < 64; i++) {
    uint16_t ind = 0;
    virtio::virtq_desc *desc = alloc_desc_chain(EVENTQ, 1, &ind);
    // printf("ind %d %p\n", ind, desc);

    desc->addr = (uint64_t)v2p(&evt[i]);
    desc->flags = VRING_DESC_F_WRITE;
    desc->len = sizeof(virtio_input_event);
    // printf("%p %d\n", desc->addr, desc->len);
    desc->next = 0;
    submit_chain(EVENTQ, ind);
  }
  kick(EVENTQ);
  return true;
}

void virtio_mmio_input::irq(int ring_index, virtio::virtq_used_elem *) { printf("irq in %d\n", ring_index); }


uint8_t virtio_mmio_input::cfg_select(uint8_t select, uint8_t subsel) {
  config().select = select;
  config().subsel = subsel;

  return config().size;
}
