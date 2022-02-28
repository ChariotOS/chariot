#pragma once

#include <types.h>
#include <fs/Node.h>
#include <mm.h>


/**
 * functions to interact with a block devices and the block cache
 */
namespace block {

  // a buffer represents a page (4k) in a block device.
  struct Buffer {
    fs::BlockDeviceNode &bdev; /* the device this buffer belongs to */

    static struct Buffer *get(fs::BlockDeviceNode &, off_t page);

    static void release(struct Buffer *);

    void register_write(void);

    ck::ref<mm::Page> page(void);

    // return the backing page data.
    void *data(void);
    off_t index(void);
    int flush(void);
    inline uint64_t last_used(void) { return m_last_used; }
    inline auto owners(void) {
      return m_count;  // XXX: race condition (!?!?)
    }

    size_t reclaim(void);

    inline bool dirty(void) { return m_dirty; }

   protected:
    inline static void release(struct blkdev *d) {}

    Buffer(fs::BlockDeviceNode &, off_t);

    bool m_dirty = false;
    spinlock m_lock;
    int m_size = PGSIZE;
    uint64_t m_count = 0;
    off_t m_index;
    uint64_t m_last_used = 0;
    ck::ref<mm::Page> m_page;
  };


  size_t reclaim_memory(void);
  void sync_all(void);

};  // namespace block


// read data from blocks to from a byte offset. These can be somewhat wasteful,
// but that gets amortized by the block flush daemon :^)
int bread(fs::BlockDeviceNode &, void *dst, size_t size, off_t byte_offset);
int bwrite(fs::BlockDeviceNode &, void *data, size_t size, off_t byte_offset);

// reclaim some memory

inline auto bget(fs::BlockDeviceNode &b, off_t page) { return block::Buffer::get(b, page); }

// release a block
static inline auto bput(struct block::Buffer *b) { return block::Buffer::release(b); }


struct bref {
  static inline bref get(fs::BlockDeviceNode &b, off_t page) { return block::Buffer::get(b, page); }

  inline bref(struct block::Buffer *b) { buf = b; }

  inline ~bref(void) {
    if (buf) bput(buf);
  }

  inline struct block::Buffer *operator->(void) { return buf; }
  inline struct block::Buffer *get(void) { return buf; }


 private:
  struct block::Buffer *buf = NULL;
};