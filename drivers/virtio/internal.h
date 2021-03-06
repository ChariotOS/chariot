#pragma once

#include <dev/disk.h>
#include <dev/virtio/mmio.h>
#include <dev/virtio/disk.h>
#include <dev/virtio/gpu.h>

#include <errno.h>
#include <phys.h>
#include <printk.h>
#include <time.h>
#include <util.h>





static inline unsigned log2_uint(unsigned val) {
  if (val == 0) return 0;  // undefined
  unsigned int count = 0;
  while (val >>= 1) {
    ++count;
  }
  return count;
}


static inline unsigned vring_size(unsigned int num, unsigned long align) {
  return ((sizeof(struct virtio::virtq_desc) * num + sizeof(uint16_t) * (3 + num) + align - 1) &
          ~(align - 1)) +
         sizeof(uint16_t) * 3 + sizeof(struct virtio::virtq_used_elem) * num;
}


static inline void vring_init(struct vring *vr, unsigned int num, void *p, unsigned long align) {
  vr->num = num;
  vr->num_mask = (1 << log2_uint(num)) - 1;
  vr->free_list = 0xffff;
  vr->free_count = 0;
  vr->last_used = 0;
  vr->desc = (virtio::virtq_desc *)p;
  vr->avail = (virtio::virtq_avail *)((off_t)p + num * sizeof(virtio::virtq_desc));
  vr->used =
      (virtio::virtq_used *)(((unsigned long)&vr->avail->ring[num] + sizeof(uint16_t) + align - 1) &
                             ~(align - 1));
}



extern void virtio_irq_handler(int i, reg_t *r, void *data);


