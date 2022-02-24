#include <chan.h>
#include <errno.h>
#include <fs.h>
#include <kshell.h>
#include <ck/map.h>
#include <mm.h>
#include <module.h>
#include <sched.h>
#include <template_lib.h>


static spinlock g_next_block_lru_lock;
static uint64_t g_next_block_lru = 0;

static uint64_t next_block_lru(void) {
  g_next_block_lru_lock.lock();
  auto l = g_next_block_lru++;
  g_next_block_lru_lock.unlock();
  return l;
}


static chan<block::Buffer *> dirty_buffers;
static int block_flush_task(void *) {
  while (1) {
    auto buffer = dirty_buffers.recv();
    buffer->flush();
    // 'recursively' put the buffer
    bput(buffer);
  }
}




static spinlock buffer_cache_lock;
static uint64_t total_blocks_in_cache = 0;
static ck::map<fs::BlockDeviceNode *, ck::map<off_t, block::Buffer *>> buffer_cache;



static uint32_t to_key(dev_t device) { return ((uint32_t)device.major() << 16) | ((uint32_t)device.minor()); }


size_t block::reclaim_memory(void) {
  size_t reclaimed = 0;
  buffer_cache_lock.lock();

  for (auto &blk : buffer_cache) {
    for (auto &off : blk.value) {
      if (off.value) {
        reclaimed += off.value->reclaim();
      }
    }
  }

  buffer_cache_lock.unlock();
  return reclaimed;
}


void block::sync_all(void) {
  buffer_cache_lock.lock();

  for (auto &blk : buffer_cache) {
    for (auto &off : blk.value) {
      if (off.value && off.value->dirty()) {
        off.value->flush();
      }
    }
  }

  buffer_cache_lock.unlock();
}

struct block_cache_key {};

namespace block {

  Buffer::Buffer(fs::BlockDeviceNode &bdev, off_t index) : bdev(bdev), m_index(index) {
    // we don't allocate the page here, only on calls to ::data().
    // start with 0 refs
    m_count = 0;
  }


  struct Buffer *block::Buffer::get(fs::BlockDeviceNode &device, off_t page) {
    scoped_irqlock l(buffer_cache_lock);

    struct block::Buffer *buf = NULL;

    auto &dev_map = buffer_cache[&device];
    buf = dev_map[page];

    if (buf == NULL) {
      buf = new block::Buffer(device, page);
      buf->m_index = page;
      dev_map[page] = buf;
      total_blocks_in_cache += 1;
    }

    buf->m_lock.lock();
    buf->m_count++;  // someone now has a copy of the buffer :^)
    buf->m_index = page;
    buf->m_last_used = next_block_lru();
    buf->m_lock.unlock();
    return buf;
  }

  void Buffer::register_write(void) { m_dirty = true; }

  void Buffer::release(struct Buffer *b) {
    b->m_lock.lock();

    if (b->m_dirty) {
      b->m_lock.unlock();
      dirty_buffers.send(move(b));
      return;
    }


    // count is decremented here iff it is not dirty
    b->m_count--;

#if CONFIG_LOW_MEMORY
    if (b->m_count == 0) {
      b->m_page = nullptr;  // release the page
                            // return PGSIZE;
    }
#endif

    b->m_lock.unlock();
  }

  int Buffer::flush(void) {
    scoped_lock l(m_lock);

    // flush even if we aren't dirty.
    if (m_page) {
      auto bsize = bdev.block_size();
      int blocks = PGSIZE / bsize;
      auto *buf = (char *)p2v(m_page->pa());

      for (int i = 0; i < blocks; i++) {
        bdev.write_block(buf + (bsize * i), m_index * blocks + i);
      }
    }
    // we're no longer dirty!
    m_dirty = false;
    return 0;
  }

  size_t Buffer::reclaim(void) {
    scoped_lock l(m_lock);
    if (m_count == 0 && m_page && !m_dirty) {
      m_page = nullptr;  // release the page
      return PGSIZE;
    }
    return 0;
  }



  void *Buffer::data(void) {
    scoped_lock l(m_lock);
    if (!m_page) {
      // get the page if there isn't one and read the blocks
      m_page = mm::Page::alloc();
      m_page->fset(PG_BCACHE);

      int blocks = PGSIZE / bdev.block_size();
      auto *buf = (char *)p2v(m_page->pa());

      for (int i = 0; i < blocks; i++) {
        auto res = bdev.read_block(buf + (bdev.block_size() * i), m_index * blocks + i);
      }
    }


    if (m_page && m_page->pa()) {
      return p2v(m_page->pa());
    }
    return nullptr;
  }


  ck::ref<mm::Page> Buffer::page(void) {
    // ::data() asserts that the page is there.
    (void)this->data();
    assert(m_page);
    return m_page;
  }

}  // namespace block


static ssize_t block_rw(fs::BlockDeviceNode &b, void *dst, size_t size, off_t byte_offset, bool write) {
  // how many more bytes are needed
  long to_access = size;
  // the offset within the current page
  ssize_t offset = byte_offset % PGSIZE;

  char *udata = (char *)dst;

  for (off_t blk = byte_offset / PGSIZE; true; blk++) {
    // get the block we are looking at.
    auto block = bget(b, blk);
    auto data = (char *)block->data();

    size_t space_left = PGSIZE - offset;
    size_t can_access = min(space_left, to_access);

    if (write) {
      memcpy(data + offset, udata, can_access);
      block->register_write();
    } else {
      memcpy(udata, data + offset, can_access);
    }

    // release the block
    bput(block);

    // moving on to the next block, we reset the offset
    offset = 0;
    to_access -= can_access;
    udata += can_access;

    if (to_access <= 0) break;
  }




  return size;
}


int bread(fs::BlockDeviceNode &b, void *dst, size_t size, off_t byte_offset) {
  return block_rw(b, dst, size, byte_offset, false /* read */);
}


int bwrite(fs::BlockDeviceNode &b, void *data, size_t size, off_t byte_offset) {
  return block_rw(b, data, size, byte_offset, true /* write */);
}



static unsigned long blk_kshell(ck::vec<ck::string> &args, void *data, int dlen) {
  if (args.size() > 0) {
    if (args[0] == "reclaim") {
      auto reclaimed = block::reclaim_memory();
      printk("reclaimed %zu bytes (%d pages)\n", reclaimed, reclaimed / PGSIZE);

      return reclaimed;
    }


    if (args[0] == "dump") {
      buffer_cache_lock.lock();

      for (auto &blk : buffer_cache) {
        for (auto &off : blk.value) {
          if (off.value) {
            printk("off: %p\n", off.value);
          }
        }
      }

      buffer_cache_lock.unlock();

      return 0;
    }
  }

  return 0;
}
static void block_init(void) {
  sched::proc::create_kthread("[block flush]", block_flush_task);
  kshell::add("blk", "blk [reclaim, dump]", blk_kshell);
}

module_init("block", block_init);
