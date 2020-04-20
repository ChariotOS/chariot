#include <asm.h>
#include <dev/RTC.h>
#include <dev/disk.h>
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
#define EXT2_CACHE_SIZE 128

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

extern fs::file_operations ext2_file_ops;
extern fs::dir_operations ext2_dir_ops;

struct [[gnu::packed]] block_group_desc {
  uint32_t block_bitmap;
  uint32_t inode_bitmap;
  uint32_t inode_table;
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

fs::ext2::ext2(void) { TRACE; }

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

bool fs::ext2::init(fs::blkdev *bdev) {
  TRACE;


	fs::blkdev::acquire(bdev);
	disk = fs::bdev_to_file(bdev);

  if (!disk) return false;

  sb = new superblock();
  // read the superblock
  disk->seek(1024, SEEK_SET);
  bool res = disk->read(sb, 1024);

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

  sb->last_mount = dev::RTC::now();
  // solve for the filesystems block size
  block_size = 1024 << sb->blocksize_hint;

  // the number of block groups can be found by rounding up the
  // blocks/blocks_per_group
  blockgroups = ceil((double)sb->blocks / (double)sb->blocks_in_blockgroup);

  first_bgd = block_size == 1024 ? 2 : 1;

  // allocate a block for the work_buf
  work_buf = kmalloc(block_size);
  inode_buf = kmalloc(block_size);

  root = get_inode(2);
  fs::inode::acquire(root);

  if (!write_superblock()) {
    printk("failed to write superblock\n");
    return false;
  }

	/*
  auto uuid = sb->s_uuid;
  u16 *u_shrts = (u16 *)(uuid + sizeof(u32));
  u64 trail = 0xFFFFFFFFFFFF & *(u64 *)(uuid + sizeof(u32) + 3 * sizeof(u16));
  KINFO("ext2 uuid: %08x-%04x-%04x-%04x-%012x\n", *(u32 *)uuid, u_shrts[0],
	u_shrts[1], u_shrts[2], trail);
  printk("block_size = %u\n", block_size);
  printk("blks in group = %u\n", sb->blocks_in_blockgroup);
  printk("total inodes = %u\n", sb->inodes);
  printk("total blocks = %u\n", sb->blocks);
	*/

  return true;
}

int fs::ext2::write_superblock(void) {
  // TODO: lock
  disk->seek(1024, SEEK_SET);
  return disk->write(sb, 1024);
}

bool fs::ext2::read_inode(ext2_inode_info &dst, u32 inode) {
  TRACE;
  u32 bg = (inode - 1) / sb->inodes_in_blockgroup;

  // now that we have which BGF the inode is in, load that desc
  read_block(first_bgd, work_buf);

  auto *bgd = (block_group_desc *)work_buf + bg;

  // find the index and seek to the inode
  u32 index = (inode - 1) % sb->inodes_in_blockgroup;
  u32 block = (index * sb->s_inode_size) / block_size;

  read_block(bgd->inode_table + block, inode_buf);

  auto *_inode =
      (ext2_inode_info *)inode_buf + (index % (block_size / sb->s_inode_size));

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
  u32 block = (index * sb->s_inode_size) / block_size;

  // read the inode buffer
  read_block(bgd->inode_table + block, inode_buf);

  // modify it...
  auto *_inode =
      (ext2_inode_info *)inode_buf + (index % (block_size / sb->s_inode_size));
  memcpy(_inode, &src, sizeof(ext2_inode_info));

  // and write the block back
  write_block(bgd->inode_table + block, inode_buf);
  return true;
}

#define BLOCKBIT(buf, n) (buf[((n) >> 3)] & (1 << (((n) % 8))))
long fs::ext2::allocate_inode(void) {
  scoped_lock l(bglock);
  int bgs = blockgroups;
  // TODO: we only support 32 block groups
  if (bgs > 32) bgs = 32;
  bool flush_changes = false;
  int res = -1;

  // now that we have which BGF the inode is in, load that desc
  read_block(first_bgd, work_buf);

  // space for the bitmap (a little wasteful with memory, but fast)
  //    (allocates a full page)
  auto vbitmap = phys::kalloc(1);

  int nfree = 0;

  for (int i = 0; i < bgs; i++) {
    auto *bgd = (block_group_desc *)work_buf + i;

    if (res == -1 && bgd->num_of_unalloc_inode > 0) {
      read_block(bgd->inode_bitmap, vbitmap);
      auto bitmap = (char *)vbitmap;

      int j = 0;
      for (; j < sb->inodes_in_blockgroup && BLOCKBIT(bitmap, j); j++) {
      }
      // evaluate to the actual inode number
      res = j + i * sb->inodes_in_blockgroup + 1;
      bitmap[j / 8] |= static_cast<u8>((1u << (j % 8)));
      write_block(bgd->inode_bitmap, vbitmap);
      sb->unallocatedinodes--;
      bgd->num_of_unalloc_inode--;
      flush_changes = true;
    }

    nfree += bgd->num_of_unalloc_inode;
  }

  printk("free inodes now: %d\n", sb->unallocatedinodes);

  if (flush_changes) {
    write_block(first_bgd, work_buf);
    write_superblock();
  }

  phys::kfree(vbitmap, 1);

  return res;
}

/* does not take a cache lock! */
struct fs::ext2_block_cache_line *fs::ext2::get_cache_line(int cba) {
  int oldest = -1;
  unsigned int oldest_age = 4294967295UL;

  for (int i = 0; i < cache_size; i++) {
    if (disk_cache[i].cba == cba) {
      return &disk_cache[i];
    }
    if (disk_cache[i].last_used < oldest_age) {
      /* We found an older block, remember this. */
      oldest = i;
      oldest_age = disk_cache[i].last_used;
    }
  }
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

  if (block_size == PGSIZE) {
    cba = block;
    cbo = 0;
  }

  auto cl = get_cache_line(cba);

  if (cl->dirty && cl->cba != cba) {
    // flush
    disk->seek(cl->cba * PGSIZE, SEEK_SET);
    disk->write(cl->buffer, PGSIZE);
    cl->dirty = false;
  }

  if (cl->cba == cba) {
    cl->last_used = cache_time++;
    memcpy(buf, cl->buffer + (cbo * block_size), block_size);
    return true;
  }

  // read into the cache line we found
  cl->cba = cba;
  cl->last_used = cache_time++;
  cl->dirty = 0;

  disk->seek(cl->cba * PGSIZE, SEEK_SET);
  bool valid = disk->read(cl->buffer, PGSIZE);
  memcpy(buf, cl->buffer + (block_size * cbo), block_size);
  return valid;

#else

  disk->seek(block * block_size, SEEK_SET);
  return disk->read(buf, block_size);

#endif
}

bool fs::ext2::write_block(u32 block, const void *buf) {
#ifdef USE_CACHE
  // I am not sure if this write code is correct
  // scoped_lock l(cache_lock);
  int cba = block >> 2;
  int cbo = block & 0x3;
  if (block_size == PGSIZE) {
    cba = block;
    cbo = 0;
  }

  auto cl = get_cache_line(cba);

  if (cl->dirty && cl->cba != cba) {
    // flush
    disk->seek(cl->cba * PGSIZE, SEEK_SET);
    disk->write(cl->buffer, PGSIZE);
    cl->dirty = false;
  }

  if (cl->cba == cba) {
    cl->last_used = cache_time++;
    memcpy(cl->buffer + (cbo * block_size), buf, block_size);

    disk->seek(block * block_size, SEEK_SET);
    return disk->write((void *)buf, block_size);
  }

  cl->cba = cba;
  cl->last_used = cache_time++;
  cl->dirty = 1;
  disk->seek(cba * PGSIZE, SEEK_SET);
  bool valid = disk->read(cl->buffer, PGSIZE);
  memcpy(cl->buffer + (cbo * block_size), buf, block_size);

  if (valid) {
    disk->seek(block * block_size, SEEK_SET);
    return disk->write((void *)buf, block_size);
  }
  return valid;

#else

  disk->seek(block * block_size, SEEK_SET);
  return disk->write((void *)buf, block_size);

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

  void *buffer = kmalloc(block_size);
  traverse_blocks(blocks_for_inode(inode), buffer, [&](void *buffer) -> bool {
    auto *entry = reinterpret_cast<ext2_dir *>(buffer);
    while ((u64)entry < (u64)buffer + block_size) {
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
    inodes[index] = fs::ext2_inode::create(this, index);
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

  u32 block_count = inode.size / block_size;
  if (inode.size % block_size != 0) block_count++;

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
    u32 *array_block = (u32 *)kmalloc(block_size);
    read_block(array_block_index, array_block);
    auto *array = reinterpret_cast<const u32 *>(array_block);
    unsigned count = min(blocks_remaining, block_size / sizeof(u32));
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

int ext2_sb_init(struct fs::superblock &sb) { return -ENOTIMPL; }

int ext2_write_super(struct fs::superblock &sb) { return -ENOTIMPL; }

int ext2_sync(struct fs::superblock &sb, int flags) { return -ENOTIMPL; }

struct fs::sb_operations ext2_ops {
  .init = ext2_sb_init, .write_super = ext2_write_super, .sync = ext2_sync,
};

static struct fs::superblock *ext2_mount(struct fs::sb_information *,
					 const char *args, int flags,
					 const char *device) {
  struct fs::blkdev *bdev = fs::bdev_from_path(device);
  if (bdev == NULL) return NULL;

  auto *sb = new fs::ext2();

  if (!sb->init(bdev)) {
		delete sb;
		return NULL;
	}

  return sb;
}

struct fs::sb_information ext2_info {
  .name = "ext2", .mount = ext2_mount, .ops = ext2_ops,
};

static void ext2_init(void) { vfs::register_filesystem(ext2_info); }

module_init("fs::ext2", ext2_init);
