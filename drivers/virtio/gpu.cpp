#include <dev/virtio_mmio.h>
#include "internal.h"



static void dump_gpu_config(const volatile struct virtio_gpu_config *config) {
  printk(KERN_INFO "[gpu] events_read 0x%llx\n", config->events_read);
  printk(KERN_INFO "[gpu] events_clear 0x%llx\n", config->events_clear);
  printk(KERN_INFO "[gpu] num_scanouts 0x%llx\n", config->num_scanouts);
  printk(KERN_INFO "[gpu] reserved 0x%llx\n", config->reserved);
}

virtio_mmio_gpu::virtio_mmio_gpu(volatile uint32_t *regs) : virtio_mmio_dev(regs) {
  uint32_t status = 0;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  write_reg(VIRTIO_MMIO_STATUS, status);

  status |= VIRTIO_CONFIG_S_DRIVER;
  write_reg(VIRTIO_MMIO_STATUS, status);

  uint64_t features = read_reg(VIRTIO_MMIO_DEVICE_FEATURES);
  /* TODO: actually negotiate features */
  printk(KERN_INFO "[gpu] features: %llx\n", features);
  write_reg(VIRTIO_MMIO_DRIVER_FEATURES, features);

  irq::install(1, virtio_irq_handler, "virtio gpu", (void *)this);


  dump_gpu_config(gpu_config());

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  write_reg(VIRTIO_MMIO_STATUS, status);


  gpu_request = phys::kalloc(1);
  gpu_request_phys = (off_t)v2p(gpu_request);
}

bool virtio_mmio_gpu::start(void) {
  get_display_info();
  return true;
}


int virtio_mmio_gpu::get_display_info(void) {
  scoped_lock l(lock);


  /* construct the get display info message */
  struct virtio_gpu_ctrl_hdr req;
  memset(&req, 0, sizeof(req));
  req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

  return 0;
}


int virtio_mmio_gpu::send_command_response(const void *cmd, size_t cmd_len, void **_res,
                                           size_t res_len) {
	assert(cmd);
	assert(_res);
	assert(cmd_len + res_len < PGSIZE);

  return 0;
}


