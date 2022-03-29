#include <dev/virtio/gpu.h>
#include <dev/virtio/mmio.h>
#include <sleep.h>
#include <util.h>
#include "internal.h"


static void dump_gpu_config(const volatile struct virtio_gpu_config *config) {
  printf(KERN_INFO "[gpu] events_read 0x%llx\n", config->events_read);
  printf(KERN_INFO "[gpu] events_clear 0x%llx\n", config->events_clear);
  printf(KERN_INFO "[gpu] num_scanouts 0x%llx\n", config->num_scanouts);
  printf(KERN_INFO "[gpu] reserved 0x%llx\n", config->reserved);
}

virtio_mmio_gpu::virtio_mmio_gpu(volatile uint32_t *regs) : virtio_mmio_dev(regs) {}


bool virtio_mmio_gpu::initialize(const struct virtio_config &config) {
  uint32_t status = 0;

  pmode_id = -1;
  next_resource_id = 1;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  write_reg(VIRTIO_MMIO_STATUS, status);

  status |= VIRTIO_CONFIG_S_DRIVER;
  write_reg(VIRTIO_MMIO_STATUS, status);

  uint64_t features = read_reg(VIRTIO_MMIO_DEVICE_FEATURES);
  /* TODO: actually negotiate features */
  printf(KERN_INFO "[gpu] features: b%llb\n", features);
  write_reg(VIRTIO_MMIO_DRIVER_FEATURES, features);

  irq::install(config.irqnr, virtio_irq_handler, "virtio gpu", (void *)this);

  // dump_gpu_config(gpu_config());

  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  write_reg(VIRTIO_MMIO_STATUS, status);


  gpu_request = phys::kalloc(1);
  gpu_request_phys = (off_t)v2p(gpu_request);

  alloc_ring(0, 16);
  // alloc_ring(2, 16);

  get_display_info();

  /* TODO: move all of this to a "set resolution" function :^) */
  if (int err = allocate_2d_resource(display_resource_id, pmode.r.width, pmode.r.height); err != 0) {
    return false;
  }

  // attach a backing store to the buffer we just created
  size_t len = pmode.r.width * pmode.r.height * 4;
  fb = (uint32_t *)phys::alloc(NPAGES(len));

  if (int err = attach_backing(display_resource_id, (void *)fb, len); err != 0) {
    return false;
  }

  if (int err = set_scanout(pmode_id, display_resource_id, pmode.r.width, pmode.r.height); err != 0) {
    return false;
  }

  {
    scoped_lock l(lock);
    auto *edid = get_edid();
  }


  // memset32(fb, 0xFF0033, pmode.r.width * pmode.r.height);
  transfer_to_host_2d(display_resource_id, pmode.r.width, pmode.r.height);
  flush_resource(display_resource_id, pmode.r.width, pmode.r.height);

  dev::video_device::register_device(this);


  return true;
}



ck::box<virtio_gpu_resource> virtio_mmio_gpu::allocate_resource(uint32_t width, uint32_t height) {
  uint32_t res_id = 0;
  /* TODO: move all of this to a "set resolution" function :^) */
  if (int err = allocate_2d_resource(res_id, width, height); err != 0) {
    return NULL;
  }

  return ck::box(new virtio_gpu_resource(*this, res_id, width, height));
}


int virtio_mmio_gpu::transfer_to_host_2d(int resource_id, uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
  int err = 0;
  scoped_lock l(lock);

  struct virtio_gpu_transfer_to_host_2d req;
  memset(&req, 0, sizeof(req));

  req.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
  req.r.x = x;
  req.r.y = y;
  req.r.width = width;
  req.r.height = height;
  req.offset = 0;
  req.resource_id = resource_id;

  /* send the command and get a response */
  struct virtio_gpu_ctrl_hdr *res;
  err = send_command(req, res);
  assert(err == 0);

  err = (res->type == VIRTIO_GPU_RESP_OK_NODATA) ? 0 : -ENOMEM;
  return err;
}

int virtio_mmio_gpu::flush_resource(int resource_id, uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
  int err = 0;
  scoped_lock l(lock);

  /* construct the request */
  struct virtio_gpu_resource_flush req;
  memset(&req, 0, sizeof(req));

  req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
  req.r.x = x;
  req.r.y = y;
  req.r.width = width;
  req.r.height = height;
  req.resource_id = resource_id;

  struct virtio_gpu_ctrl_hdr *res;
  err = send_command(req, res);
  assert(err == 0);

  err = (res->type == VIRTIO_GPU_RESP_OK_NODATA) ? 0 : -ENOMEM;

  return err;
}

int virtio_mmio_gpu::get_display_info(void) {
  scoped_lock l(lock);

  /* construct the get display info message */
  struct virtio_gpu_ctrl_hdr req = {0};
  memset(&req, 0, sizeof(req));
  req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

  struct virtio_gpu_resp_display_info *info = NULL;
  int err = send_command(req, info);
  if (err != 0) return err;

  if (info->hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
    hexdump(&info->hdr, sizeof(info->hdr), true);
    printf("error: %04x\n", info->hdr.type);
    return -1;
  }

  for (int i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
    if (info->pmodes[i].enabled) {
      printf("[virtio gpu] pmode[%u]: x %u y %u w %u h %u flags 0x%x\n", i, info->pmodes[i].r.x, info->pmodes[i].r.y,
          info->pmodes[i].r.width, info->pmodes[i].r.height, info->pmodes[i].flags);
      if (pmode_id < 0) {
        /* save the first valid pmode we see */
        memcpy(&pmode, &info->pmodes[i], sizeof(pmode));
        pmode_id = i;
      }
    }
  }



  return 0;
}

int virtio_mmio_gpu::set_scanout(int scanout_id, uint32_t resource_id, uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
  scoped_lock l(lock);

  struct virtio_gpu_set_scanout req;
  memset(&req, 0, sizeof(req));
  req.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
  req.r.x = x;
  req.r.y = y;
  req.r.width = width;
  req.r.height = height;
  req.scanout_id = scanout_id;
  req.resource_id = resource_id;

  struct virtio_gpu_ctrl_hdr *res;
  if (int err = send_command(req, res); err != 0) return err;

  return (res->type == VIRTIO_GPU_RESP_OK_NODATA) ? 0 : -ENOMEM;
}

int virtio_mmio_gpu::attach_backing(uint32_t resource_id, void *ptr, size_t buf_len) {
  scoped_lock l(lock);

  /* construct the request */
  struct {
    struct virtio_gpu_resource_attach_backing req;
    struct virtio_gpu_mem_entry mem;
  } req;
  memset(&req, 0, sizeof(req));

  req.req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
  req.req.resource_id = resource_id;
  req.req.nr_entries = 1;

  off_t pa = (off_t)v2p(ptr);
  req.mem.addr = pa;
  req.mem.length = buf_len;


  struct virtio_gpu_ctrl_hdr *res;
  if (int err = send_command(req, res); err != 0) return err;
  return (res->type == VIRTIO_GPU_RESP_OK_NODATA) ? 0 : -ENOMEM;
}


int virtio_mmio_gpu::send_command_raw(const void *cmd, size_t cmd_len, void **_res, size_t res_len) {
  scoped_lock _iolock(iolock);
  assert(cmd);
  assert(_res);
  assert(cmd_len + res_len < PGSIZE);

  uint16_t i;
  struct virtio::virtq_desc *desc = alloc_desc_chain(0, 2, &i);
  assert(desc);
  desc->addr = gpu_request_phys;
  desc->len = cmd_len;
  desc->flags |= VRING_DESC_F_NEXT;

  memcpy(gpu_request, cmd, cmd_len);

  /* Set the second descriptor to the resonse with the write bit set */
  desc = index_to_desc(0, desc->next);
  assert(desc);

  void *res = (void *)((uint8_t *)gpu_request + cmd_len);
  *_res = res;
  off_t res_phys = gpu_request_phys + cmd_len;
  memset(res, 0, res_len);

  desc->addr = res_phys;
  desc->len = res_len;
  desc->flags = VRING_DESC_F_WRITE;

  __sync_synchronize();

  // prep a wait for an irq :)
  struct wait_entry ent;
  prepare_to_wait(iowait, ent);

  // submit the request
  submit_chain(0, i);
  // kick off the request by banging on the register
  kick(0);

  ent.start();

  return 0;
}


int virtio_mmio_gpu::allocate_2d_resource(uint32_t &id_out, uint32_t width, uint32_t height) {
  scoped_lock l(lock);

  struct virtio_gpu_resource_create_2d req;
  memset(&req, 0, sizeof(req));

  req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
  req.resource_id = next_resource_id++;
  id_out = req.resource_id;
  req.format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
  req.width = width;
  req.height = height;

  struct virtio_gpu_ctrl_hdr *res = NULL;
  if (auto err = send_command(req, res); err != 0) {
    return err;
  }

  // printf("response type 0x%x\n", res->type);
  return (res->type == VIRTIO_GPU_RESP_OK_NODATA) ? 0 : -ENOMEM;
}

void virtio_mmio_gpu::irq(int ring, virtio::virtq_used_elem *e) {
  /* parse our descriptor chain, add back to the free queue */
  uint16_t i = e->id;
  for (;;) {
    int next;
    auto *desc = index_to_desc(ring, i);

    if (desc->flags & VRING_DESC_F_NEXT) {
      next = desc->next;
    } else {
      /* end of chain */
      next = -1;
    }

    free_desc(ring, i);

    if (next < 0) break;
    i = next;
  }
  iowait.wake_up();
}


struct virtio_gpu_resp_edid *virtio_mmio_gpu::get_edid(void) {
  struct virtio_gpu_get_edid req;
  memset(&req, 0, sizeof(req));

  req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
  req.scanout = pmode_id;
  req.padding = 0;  // unsure.

  struct virtio_gpu_resp_edid *res = NULL;
  if (auto err = send_command(req, res); err != 0) {
    return NULL;
  }

  return res;
}




virtio_gpu_resource::virtio_gpu_resource(virtio_mmio_gpu &gpu, uint32_t id, uint32_t width, uint32_t height)
    : id(id), width(width), height(height), gpu(gpu) {
  x = 0;
  y = 0;

  auto pages = round_up(width * height * 4, 4096) >> 12;

  fb = (uint32_t *)phys::alloc(pages);
  gpu.attach_backing(id, fb, width * height * 4);
}



int virtio_gpu_resource::transfer(void) { return gpu.transfer_to_host_2d(id, width, height, x, y); }
int virtio_gpu_resource::flush(void) { return gpu.flush_resource(id, width, height, x, y); }

virtio_gpu_resource::~virtio_gpu_resource(void) { panic("ded"); }



// default
int virtio_mmio_gpu::get_mode(gvi_video_mode &m) {
  m.width = pmode.r.width;
  m.height = pmode.r.height;

  m.caps = 0;
  m.caps |= GVI_CAP_DOUBLE_BUFFER;
  return 0;
}

int virtio_mmio_gpu::flush_fb(void) {
  transfer_to_host_2d(display_resource_id, pmode.r.width, pmode.r.height);
  flush_resource(display_resource_id, pmode.r.width, pmode.r.height);
  return 0;
}


// default
int virtio_mmio_gpu::set_mode(const gvi_video_mode &) {
  printf("set mode\n");
  return -ENOTIMPL;
}

uint32_t *virtio_mmio_gpu::get_framebuffer(void) { return fb; }
