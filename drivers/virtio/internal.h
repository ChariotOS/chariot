#pragma once

#include <dev/disk.h>
#include <dev/virtio_mmio.h>

#include <errno.h>
#include <phys.h>
#include <printk.h>
#include <time.h>
#include <util.h>


#define VIO_PGCOUNT 2
#define VIO_NUM_DESC 8
#define VIO_MAX_RINGS 4

struct vring {
  bool active = false;
  uint32_t num;
  uint32_t num_mask;

  uint16_t free_list; /* head of a free list of descriptors per ring. 0xffff is NULL */
  uint16_t free_count;

  uint16_t last_used;

  struct virtio::virtq_desc *desc;
  struct virtio::virtq_avail *avail;
  struct virtio::virtq_used *used;
};



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


class virtio_mmio_dev {
 protected:
  /* Virtual address of the registers */
  volatile uint32_t *regs = NULL;
  /* virtio communicates mostly through RAM, so this is the pointer to those pages */
  void *pages = NULL;

#if 0

  // pages[] is divided into three regions (descriptors, avail, and
  // used), as explained in Section 2.6 of the virtio specification
  // for the legacy interface.
  // https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf

  // the first region of pages[] is a set (not a ring) of DMA
  // descriptors, with which the driver tells the device where to read
  // and write individual disk operations. there are NUM descriptors.
  // most commands consist of a "chain" (a linked list) of a couple of
  // these descriptors.
  // points into pages[].
  struct virtio::virtq_desc *desc;

  // next is a ring in which the driver writes descriptor numbers
  // that the driver would like the device to process.  it only
  // includes the head descriptor of each chain. the ring has
  // NUM elements.
  // points into pages[].
  struct virtio::virtq_avail *avail;

  // finally a ring in which the device writes descriptor numbers that
  // the device has finished processing (just the head of each chain).
  // there are NUM used ring entries.
  // points into pages[].
  struct virtio::virtq_used *used;

  bool free[VIO_NUM_DESC]; /* Is a descriptor free? */
  uint16_t used_idx;       // we've looked this far in used[2..NUM].

#endif
 public:
  struct vring ring[VIO_MAX_RINGS];

  spinlock vdisk_lock;

  inline auto &mmio_regs(void) {
    return *(virtio::mmio_regs *)regs;
  }
  inline volatile uint32_t read_reg(int off) {
    return *(volatile uint32_t *)((off_t)regs + off);
  }
  inline void write_reg(int off, uint32_t val) {
    *(volatile uint32_t *)((off_t)regs + off) = val;
  }
  virtio_mmio_dev(volatile uint32_t *regs);
  virtual ~virtio_mmio_dev(void);

  void irq(void);
	virtual void irq(int ring_index, virtio::virtq_used_elem *) {}

  int alloc_ring(int index, int len);

  // find a free descriptor, mark it non-free, return its index.
  uint16_t alloc_desc(int ring_index);

  // mark a descriptor as free.
  void free_desc(int ring_index, int i);
  void submit_chain(int ring_index, int desc_index);

  virtio::virtq_desc *alloc_desc_chain(int ring_index, int count, uint16_t *start_index);
  inline virtio::virtq_desc *index_to_desc(int ring_index, int index) {
    auto d = &ring[ring_index].desc[index];
		return d;
  }
};


extern void virtio_irq_handler(int i, reg_t *r, void *data);


class virtio_mmio_disk : public virtio_mmio_dev, public dev::disk {
  // track info about in-flight operations,
  // for use when completion interrupt arrives.
  // indexed by first descriptor index of chain.
  struct {
    void *data;
    char status;
    wait_queue wq;
  } info[VIO_NUM_DESC];

  // disk command headers.
  // one-for-one with descriptors, for convenience.
  struct virtio::blk_req *ops = NULL;
  spinlock vdisk_lock;

 public:
  virtio_mmio_disk(volatile uint32_t *regs);
	virtual void irq(int ring_index, virtio::virtq_used_elem *);

  void disk_rw(uint32_t sector, void *data, int n, int write);

  virtual size_t block_size();
  virtual size_t block_count();
  virtual bool read_blocks(uint32_t sector, void *data, int nsec = 1);
  virtual bool write_blocks(uint32_t sector, const void *data, int nsec = 1);


  inline auto &config(void) {
    return *(virtio::blk_config *)((off_t)this->regs + 0x100);
  }
};


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


class virtio_mmio_gpu : public virtio_mmio_dev {
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
  void *fb;

 public:
  virtio_mmio_gpu(volatile uint32_t *regs);

  inline struct virtio_gpu_config *gpu_config(void) {
    return (virtio_gpu_config *)((off_t)this->regs + 0x100);
  }

  /* Tell the GPU to start up :) (load a default framebuffer) */
  bool start(void);

  /* Update the pmode field with the current mode */
  int get_display_info(void);

  int send_command_response(const void *cmd, size_t cmd_len, void **_res, size_t res_len);
};
