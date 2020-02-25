#include <asm.h>
#include <dev/RTC.h>
#include <dev/blk_dev.h>
#include <errno.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
#include <math.h>
#include <mem.h>
#include <module.h>
#include <phys.h>
#include <string.h>
#include <util.h>

// Standard information and structures for EXT2
#define EXT2_SIGNATURE 0xEF53

// #define EXT2_DEBUG
// #define EXT2_TRACE
#define USE_CACHE

#ifdef EXT2_DEBUG
#define INFO(fmt, args...) printk("[EXT2] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

#ifdef EXT2_TRACE
#define TRACE INFO("TRACE: (%d) %s\n", __LINE__, __PRETTY_FUNCTION__)
#else
#define TRACE
#endif

struct [[gnu::packed]] block_group_desc {
  uint32_t block_of_block_usage_bitmap;
  uint32_t block_of_inode_usage_bitmap;
  uint32_t block_of_inode_table;
  uint16_t num_of_unalloc_block;
  uint16_t num_of_unalloc_inode;
  uint16_t num_of_dirs;
  uint8_t unused[14];
};

typedef struct __ext2_dir_entry {
  uint32_t inode;
  uint16_t size;
  uint8_t namelength;
  uint8_t reserved;
  char name[0];
  /* name here */
} __attribute__((packed)) ext2_dir;

#define EXT2_CACHE_SIZE 128

fs::ext2::ext2(fs::filedesc disk) : filesystem(/*super*/), disk(disk) { TRACE; }

fs::ext2::~ext2(void) {
  TRACE;
  if (sb != nullptr) delete sb;
  if (work_buf != nullptr) kfree(work_buf);
  if (inode_buf != nullptr) kfree(inode_buf);

#ifdef USE_CACHE
  for (int i = 0; i < cache_size; i++) {
    if (disk_cache[i].buffer != NULL) {
      phys::free(v2p(disk_cache[i].buffer));
    }
  }
  delete[] disk_cache;
#endif
}

bool fs::ext2::init(void) {
  TRACE;

  if (!disk) return false;

  sb = new superblock();
  // read the superblock
  disk.seek(1024, SEEK_SET);
  bool res = disk.read(sb, 1024);

  if (!res) {
    printk("failed to read the superblock\n");
    return false;
  }

  // first, we need to make 100% sure this is actually a disk with ext2 on it...
  if (sb->ext2_sig != 0xef53) {
    printk("Block device does not contain the ext2 signature\n");
    return false;
  }

#ifdef USE_CACHE
  cache_size = EXT2_CACHE_SIZE;
  disk_cache = new ext2_block_cache_line[cache_size];
  for (int i = 0; i < cache_size; i++) {
    disk_cache[i].cba = -1;
    disk_cache[i].dirty = false;
    disk_cache[i].buffer = NULL;
  }
#endif
  sb->last_check = dev::RTC::now();

  // solve for the filesystems block size
  blocksize = 1024 << sb->blocksize_hint;

  // the number of block groups can be found by rounding up the
  // blocks/blocks_per_group
  blockgroups = ceil((double)sb->blocks / (double)sb->blocks_in_blockgroup);

  first_bgd = sb->superblock_id + (sizeof(fs::ext2::superblock) / blocksize);

  // allocate a block for the work_buf
  work_buf = kmalloc(blocksize);
  inode_buf = kmalloc(blocksize);

  root = get_inode(2);
  fs::inode::acquire(root);

  if (!write_superblock()) {
    printk("failed to write superblock\n");
    return false;
  }

  auto uuid = sb->s_uuid;

  u16 *u_shrts = (u16 *)(uuid + sizeof(u32));

  u64 trail = 0xFFFFFFFFFFFF & *(u64 *)(uuid + sizeof(u32) + 3 * sizeof(u16));
  KINFO("ext2 uuid: %08x-%04x-%04x-%04x-%012x\n", *(u32 *)uuid, u_shrts[0],
        u_shrts[1], u_shrts[2], trail);

  return true;
}

int fs::ext2::write_superblock(void) {
  // TODO: lock
  disk.seek(1024, SEEK_SET);
  return disk.write(sb, 1024);
}

bool fs::ext2::read_inode(ext2_inode_info &dst, u32 inode) {
  TRACE;
  u32 bg = (inode - 1) / sb->inodes_in_blockgroup;

  // now that we have which BGF the inode is in, load that desc
  read_block(first_bgd, work_buf);

  auto *bgd = (block_group_desc *)work_buf + bg;

  // find the index and seek to the inode
  u32 index = (inode - 1) % sb->inodes_in_blockgroup;

  u32 block = (index * sb->s_inode_size) / blocksize;

  read_block(bgd->block_of_inode_table + block, inode_buf);

  auto *_inode =
      (ext2_inode_info *)inode_buf + (index % (blocksize / sb->s_inode_size));

  memcpy(&dst, _inode, sizeof(ext2_inode_info));

  return true;
}

bool fs::ext2::write_inode(ext2_inode_info &src, u32 inode) {
  TRACE;
  u32 bg = (inode - 1) / sb->inodes_in_blockgroup;

  // now that we have which BGF the inode is in, load that desc
  read_block(first_bgd, work_buf);

  auto *bgd = (block_group_desc *)work_buf + bg;

  // find the index and seek to the inode
  u32 index = (inode - 1) % sb->inodes_in_blockgroup;
  u32 block = (index * sb->s_inode_size) / blocksize;

  // read the inode buffer
  read_block(bgd->block_of_inode_table + block, inode_buf);

  // modify it...
  auto *_inode =
      (ext2_inode_info *)inode_buf + (index % (blocksize / sb->s_inode_size));
  memcpy(_inode, &src, sizeof(ext2_inode_info));

  // and write the block back
  write_block(bgd->block_of_inode_table + block, inode_buf);
  return true;
}

/* does not take a cache lock! */
struct fs::ext2_block_cache_line *fs::ext2::get_cache_line(int cba) {
  int oldest = -1;
  unsigned int oldest_age = 4294967295UL;
  for (int i = 0; i < cache_size; i++) {
    // printk("%2d: lu:%d, blk:%d\n", i, disk_cache[i].last_used,
    // disk_cache[i].blkno);
    if (disk_cache[i].cba == cba) {
      return &disk_cache[i];
    }
    if (disk_cache[i].last_used < oldest_age) {
      /* We found an older block, remember this. */
      oldest = i;
      oldest_age = disk_cache[i].last_used;
    }
  }
  // printk("oldest = %d\n", oldest);
  if (disk_cache[oldest].buffer == NULL) {
    disk_cache[oldest].cba = -1;
    disk_cache[oldest].dirty = false;
    disk_cache[oldest].buffer = (char *)p2v(phys::alloc());
  }

  return &disk_cache[oldest];
}

bool fs::ext2::read_block(u32 block, void *buf) {
#ifdef USE_CACHE
  scoped_lock l(cache_lock);
  int cba = block >> 2;
  int cbo = block & 0x3;
  auto cl = get_cache_line(cba);

  if (cl->dirty && cl->cba != cba) {
    // flush
    disk.seek(cl->cba * PGSIZE, SEEK_SET);
    disk.write(cl->buffer, PGSIZE);
    cl->dirty = false;
  }

  if (cl->cba == cba) {
    cl->last_used = cache_time++;
    memcpy(buf, cl->buffer + (cbo * blocksize), blocksize);
    return true;
  }

  // read into the cache line we found
  cl->cba = cba;
  cl->last_used = cache_time++;
  cl->dirty = 0;

  disk.seek(cl->cba * PGSIZE, SEEK_SET);
  bool valid = disk.read(cl->buffer, PGSIZE);

  memcpy(buf, cl->buffer + (blocksize * cbo), blocksize);
  return valid;

#else

  disk.seek(block * blocksize, SEEK_SET);
  return disk.read(buf, blocksize);

#endif
}

bool fs::ext2::write_block(u32 block, const void *buf) {


#ifdef USE_CACHE
  // I am not sure if this write code is correct
  scoped_lock l(cache_lock);
  int cba = block >> 2;
  int cbo = block & 0x3;
  auto cl = get_cache_line(cba);

  if (cl->dirty && cl->cba != cba) {
    // flush
    disk.seek(cl->cba * PGSIZE, SEEK_SET);
    disk.write(cl->buffer, PGSIZE);
    cl->dirty = false;
  }

  if (cl->cba == cba) {
    cl->last_used = cache_time++;
    memcpy(cl->buffer + (cbo * blocksize), buf, blocksize);
    return true;
  }

  cl->cba = cba;
  cl->last_used = cache_time++;
  cl->dirty = 1;
  disk.seek(cl->cba * PGSIZE, SEEK_SET);
  bool valid = disk.read(cl->buffer, PGSIZE);
  memcpy(cl->buffer + (cbo * blocksize), buf, blocksize);
  return valid;

#else

  disk.seek(block * blocksize, SEEK_SET);
  return disk.read(buf, blocksize);

#endif
}

void fs::ext2::traverse_blocks(vec<u32> blks, void *buf,
                               func<bool(void *)> callback) {
  TRACE;
  for (auto b : blks) {
    read_block(b, buf);
    if (!callback(buf)) return;
  }
}

void fs::ext2::traverse_dir(u32 inode,
                            func<bool(u32 ino, const char *name)> callback) {
  TRACE;
  ext2_inode_info i;
  read_inode(i, inode);
  traverse_dir(i, callback);
}

void fs::ext2::traverse_dir(ext2_inode_info &inode,
                            func<bool(u32 ino, const char *name)> callback) {
  TRACE;

  void *buffer = kmalloc(blocksize);
  traverse_blocks(blocks_for_inode(inode), buffer, [&](void *buffer) -> bool {
    auto *entry = reinterpret_cast<ext2_dir *>(buffer);
    while ((u64)entry < (u64)buffer + blocksize) {
      if (entry->inode != 0) {
        fs::directory_entry ent;
        ent.inode = entry->inode;

        // TODO: bad!
        char buf[entry->namelength + 1];
        memcpy(buf, entry->name, entry->namelength);
        buf[entry->namelength] = 0;
        ent.name = buf;
        if (!callback(ent.inode, buf)) break;
      }
      entry = (ext2_dir *)((char *)entry + entry->size);
    }
    return true;
  });

  kfree(buffer);
}

struct fs::inode *fs::ext2::get_root(void) {
  return root;
}

struct fs::inode *fs::ext2::get_inode(u32 index) {
  TRACE;
  scoped_lock lck(m_lock);
  if (inodes[index] == NULL) {
    inodes[index] = fs::ext2_inode::create(*this, index);
  }
  return inodes[index];
}

vec<u32> fs::ext2::blocks_for_inode(u32 inode) {
  TRACE;
  ext2_inode_info info;
  read_inode(info, inode);
  return blocks_for_inode(info);
}

vec<u32> fs::ext2::blocks_for_inode(ext2_inode_info &inode) {
  TRACE;
  u32 block_count = inode.size / blocksize;
  if (inode.size % blocksize != 0) block_count++;

  vec<u32> list;

  list.ensure_capacity(block_count);

  u32 blocks_remaining = block_count;

  u32 direct_count = min(block_count, 12);

  for (unsigned i = 0; i < direct_count; ++i) {
    auto block_index = inode.dbp[i];
    if (!block_index) return list;
    list.push(block_index);
    --blocks_remaining;
  }

  if (!blocks_remaining) return list;

  auto process_block_array = [&](unsigned array_block_index, auto &&callback) {
    u32 *array_block = (u32 *)kmalloc(blocksize);
    read_block(array_block_index, array_block);
    auto *array = reinterpret_cast<const u32 *>(array_block);
    unsigned count = min(blocks_remaining, blocksize / sizeof(u32));
    for (unsigned i = 0; i < count; ++i) {
      if (!array[i]) {
        blocks_remaining = 0;
        kfree(array_block);
        return;
      }
      callback(array[i]);
      --blocks_remaining;
    }
    kfree(array_block);
  };

  // process the singly linked block
  process_block_array(inode.singly_block, [&](unsigned entry) {
    list.push(entry);
    --blocks_remaining;
  });

  if (!blocks_remaining) return list;

  process_block_array(inode.doubly_block, [&](unsigned entry) {
    process_block_array(entry, [&](unsigned entry) {
      list.push(entry);
      --blocks_remaining;
    });
  });

  if (!blocks_remaining) return list;

  process_block_array(inode.triply_block, [&](unsigned entry) {
    process_block_array(entry, [&](unsigned entry) {
      process_block_array(entry, [&](unsigned entry) {
        list.push(entry);
        --blocks_remaining;
      });
    });
  });
  return list;
}

u32 fs::ext2::balloc(void) {
  scoped_lock l(m_lock);

  return 0;
}

void fs::ext2::bfree(u32 block) { scoped_lock l(m_lock); }

static unique_ptr<fs::filesystem> ext2_mounter(ref<dev::device>, int flags) {
  printk("trying to mount\n");
  return nullptr;
}

static void ext2_init(void) { vfs::register_filesystem("ext2", ext2_mounter); }

module_init("fs::ext2", ext2_init);
