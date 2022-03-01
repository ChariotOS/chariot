#pragma once

#include <dev/virtio/mmio.h>
#include <lock.h>
#include <wait.h>
#include <dev/video.h>

/* taken from qemu sources */
enum virtio_gpu_ctrl_type {
  VIRTIO_GPU_UNDEFINED = 0,

  /* 2d commands */
  VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
  VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
  VIRTIO_GPU_CMD_RESOURCE_UNREF,
  VIRTIO_GPU_CMD_SET_SCANOUT,
  VIRTIO_GPU_CMD_RESOURCE_FLUSH,
  VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
  VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
  VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,

  /* cursor commands */
  VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
  VIRTIO_GPU_CMD_MOVE_CURSOR,

  /* success responses */
  VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
  VIRTIO_GPU_RESP_OK_DISPLAY_INFO,

  /* error responses */
  VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
  VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
  VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
  VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
  VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
  VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

#define VIRTIO_GPU_FLAG_FENCE (1 << 0)

struct virtio_gpu_ctrl_hdr {
  uint32_t type;
  uint32_t flags;
  uint64_t fence_id;
  uint32_t ctx_id;
  uint32_t padding;
};

/* data passed in the cursor vq */

struct virtio_gpu_cursor_pos {
  uint32_t scanout_id;
  uint32_t x;
  uint32_t y;
  uint32_t padding;
};

/* VIRTIO_GPU_CMD_UPDATE_CURSOR, VIRTIO_GPU_CMD_MOVE_CURSOR */
struct virtio_gpu_update_cursor {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_cursor_pos pos; /* update & move */
  uint32_t resource_id;             /* update only */
  uint32_t hot_x;                   /* update only */
  uint32_t hot_y;                   /* update only */
  uint32_t padding;
};

/* data passed in the control vq, 2d related */

struct virtio_gpu_rect {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
};

/* VIRTIO_GPU_CMD_RESOURCE_UNREF */
struct virtio_gpu_resource_unref {
  struct virtio_gpu_ctrl_hdr hdr;
  uint32_t resource_id;
  uint32_t padding;
};

/* VIRTIO_GPU_CMD_RESOURCE_CREATE_2D: create a 2d resource with a format */
struct virtio_gpu_resource_create_2d {
  struct virtio_gpu_ctrl_hdr hdr;
  uint32_t resource_id;
  uint32_t format;
  uint32_t width;
  uint32_t height;
};

/* VIRTIO_GPU_CMD_SET_SCANOUT */
struct virtio_gpu_set_scanout {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_rect r;
  uint32_t scanout_id;
  uint32_t resource_id;
};

/* VIRTIO_GPU_CMD_RESOURCE_FLUSH */
struct virtio_gpu_resource_flush {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_rect r;
  uint32_t resource_id;
  uint32_t padding;
};

/* VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D: simple transfer to_host */
struct virtio_gpu_transfer_to_host_2d {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_rect r;
  uint64_t offset;
  uint32_t resource_id;
  uint32_t padding;
};

struct virtio_gpu_mem_entry {
  uint64_t addr;
  uint32_t length;
  uint32_t padding;
};

/* VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING */
struct virtio_gpu_resource_attach_backing {
  struct virtio_gpu_ctrl_hdr hdr;
  uint32_t resource_id;
  uint32_t nr_entries;
};

/* VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING */
struct virtio_gpu_resource_detach_backing {
  struct virtio_gpu_ctrl_hdr hdr;
  uint32_t resource_id;
  uint32_t padding;
};
struct virtio_gpu_display_one {
  struct virtio_gpu_rect r;
  uint32_t enabled;
  uint32_t flags;
};
/* VIRTIO_GPU_RESP_OK_DISPLAY_INFO */
#define VIRTIO_GPU_MAX_SCANOUTS 16
struct virtio_gpu_resp_display_info {
  struct virtio_gpu_ctrl_hdr hdr;
  struct virtio_gpu_display_one pmodes[VIRTIO_GPU_MAX_SCANOUTS];
};

struct virtio_gpu_get_edid {
  struct virtio_gpu_ctrl_hdr hdr;
  uint32_t scanout;
  uint32_t padding;
};

struct virtio_gpu_resp_edid {
  struct virtio_gpu_ctrl_hdr hdr;
  uint32_t size;
  uint32_t padding;
  uint8_t edid[1024];
};

#define VIRTIO_GPU_EVENT_DISPLAY (1 << 0)

struct virtio_gpu_config {
  uint32_t events_read;
  uint32_t events_clear;
  uint32_t num_scanouts;
  uint32_t reserved;
};

/* simple formats for fbcon/X use */
enum virtio_gpu_formats {
  VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1,
  VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM = 2,
  VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM = 3,
  VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM = 4,

  VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM = 67,
  VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM = 68,

  VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM = 121,
  VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM = 134,
};

class virtio_mmio_gpu;

class virtio_gpu_resource {
 public:
  uint32_t id;
  uint32_t width, height;
  uint32_t x, y;
  uint32_t *fb = NULL;

  virtio_mmio_gpu &gpu;

  ~virtio_gpu_resource();

  int transfer(void);
  int flush(void);
  inline uint32_t npixels(void) { return width * height; }

 protected:
  friend class virtio_mmio_gpu;
  virtio_gpu_resource(virtio_mmio_gpu &gpu, uint32_t id, uint32_t width, uint32_t height);
};


class virtio_mmio_gpu : public virtio_mmio_dev, public dev::VideoDevice {
  friend class virtio_gpu_resource;

  spinlock lock;

  void *gpu_request;
  off_t gpu_request_phys;


  /* a saved copy of the display */
  struct virtio_gpu_display_one pmode;
  int pmode_id;


  /* resource id that is set as scanout */
  uint32_t display_resource_id;

  /* next resource id */
  uint32_t next_resource_id;

  // event_t flush_event;

  /* framebuffer */
  uint32_t *fb;

  // wait on a response from the GPU
  struct wait_queue iowait;
  // to lock waiting to one task (TODO per-request waitqueues?)
  spinlock iolock;

  /* Update the pmode field with the current mode */
  int get_display_info(void);
  int allocate_2d_resource(uint32_t &id_out, uint32_t width, uint32_t height);
  int attach_backing(uint32_t resource_id, void *ptr, size_t buf_len);

  int send_command_raw(const void *cmd, size_t cmd_len, void **_res, size_t res_len);
  template <typename Rq, typename Rs>
  inline int send_command(const Rq &req, Rs *&res) {
    return send_command_raw(&req, sizeof(Rq), (void **)&res, sizeof(Rs));
  }

  int set_scanout(int pmode_id, uint32_t resource_id, uint32_t width, uint32_t height, uint32_t x = 0, uint32_t y = 0);

  int transfer_to_host_2d(int resource_id, uint32_t width, uint32_t height, uint32_t x = 0, uint32_t y = 0);
  int flush_resource(int resource_id, uint32_t width, uint32_t height, uint32_t x = 0, uint32_t y = 0);

  /* CALLER HOLDS LOCK */
  struct virtio_gpu_resp_edid *get_edid();


  ck::box<virtio_gpu_resource> allocate_resource(uint32_t width, uint32_t height);

 public:
  virtio_mmio_gpu(volatile uint32_t *regs);

  inline struct virtio_gpu_config *gpu_config(void) { return (virtio_gpu_config *)((off_t)this->regs + 0x100); }

  virtual bool initialize(const struct virtio_config &config);


  virtual void irq(int ring_index, virtio::virtq_used_elem *);


  // ^dev::video_device
  virtual int get_mode(gvi_video_mode &mode);
  virtual int set_mode(const gvi_video_mode &mode);
  virtual uint32_t *get_framebuffer(void);
  virtual int flush_fb(void);
};
