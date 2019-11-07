#include <dev/blk_cache.h>
#include <printk.h>
#include <sched.h>


static int used = 0;

dev::blk_cache::blk_cache(ref<dev::blk_dev> disk, u32 cache_size)
    : dev::blk_dev(nullptr), m_disk(disk), m_cache_size(cache_size) {
  lines = new cache_line[cache_size];
}

dev::blk_cache::~blk_cache(void) { delete[] lines; }

u64 dev::blk_cache::block_size(void) { return m_disk->block_size(); }

void dev::blk_cache::evict(u32 cache_ind) {
  auto& line = lines[cache_ind];

  if (line.dirty) {
    printk("evicting dirty line %d. sect: %d\n", cache_ind, line.sector);
  }

  line.valid = false;
  line.sector = -1;
  line.dirty = false;
}

bool dev::blk_cache::read_block(u32 sector, u8* buf) {

  // TODO: lock!
  auto cind = sector % m_cache_size;

  // printk("read(%6d) %6d@%-6d ...", nread++, sector, cind);

  auto& line = lines[cind];

  if (!line.valid || line.data == nullptr || line.sector != sector) {
    evict(cind);
    sched::beep();
    line.sector = sector;

    if (line.data == nullptr) {
      line.data = kmalloc(block_size());
      used++;
    }
    line.valid = m_disk->read_block(sector, (u8*)line.data);
    // printk(" ###");
  } else {
    // printk("    ");
  }

  if (line.valid) {
    memcpy(buf, line.data, block_size());
    // printk(" OK %d\n", used);
  } else {
    // printk("\n");
  }

  return line.valid;
}
bool dev::blk_cache::write_block(u32 sector, const u8* buf) {
  // TODO: lock!
  auto cind = sector % m_cache_size;

  // evict the line until real write caching works
  evict(cind);

  // write thru
  return m_disk->write_block(sector, buf);
}

dev::blk_cache::cache_line::cache_line(void) {
  valid = false;
  sector = -1;
  dirty = false;
  data = nullptr;
}

dev::blk_cache::cache_line::~cache_line(void) {
  if (data != nullptr) kfree(data);
}

