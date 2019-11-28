#include <asm.h>
#include <dev/RTC.h>
#include <dev/blk_dev.h>
#include <errno.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
#include <math.h>
#include <mem.h>
#include <module.h>
#include <string.h>

// Standard information and structures for EXT2
#define EXT2_SIGNATURE 0xEF53

// #define EXT2_DEBUG
// #define EXT2_TRACE

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

fs::ext2::ext2(ref<dev::device> dev)
    : filesystem(/*super*/), dev(dev), disk(dev), m_lock("ext2fs") {
  TRACE;
}

fs::ext2::~ext2(void) {
  TRACE;
  if (sb != nullptr) delete sb;
  if (work_buf != nullptr) kfree(work_buf);
  if (inode_buf != nullptr) kfree(inode_buf);
}

bool fs::ext2::init(void) {
  TRACE;

  sb = new superblock();
  // read the superblock
  bool res = dev->read(1024, 1024, sb);

  if (!res) {
    printk("failed to read the superblock\n");
    return false;
  }

  // first, we need to make 100% sure this is actually a disk with ext2 on it...
  if (sb->ext2_sig != 0xef53) {
    printk("Block device does not contain the ext2 signature\n");
    return false;
  }

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
  return disk->write(1024, 1024, sb);
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
/**
 *
 *
 *
 */

bool fs::ext2::read_block(u32 block, void *buf) {
  TRACE;
  bool valid = disk->read(block * blocksize, blocksize, buf);
  return valid;
}

bool fs::ext2::write_block(u32 block, const void *buf) {
  TRACE;
  return disk->write(block * blocksize, blocksize, buf);
}

void fs::ext2::traverse_blocks(vec<u32> blks, void *buf,
                               func<bool(void *)> callback) {
  TRACE;
  for (auto b : blks) {
    read_block(b, buf);
    if (!callback(buf)) return;
  }
}

/*
void *fs::ext2::read_entire(ext2_inode_info &inode) {
  TRACE;

  scoped_lock lck(m_lock);


  u32 block_count = inode.size / blocksize;
  if (inode.size % blocksize != 0) block_count++;

  auto buf = (char *)kmalloc(block_count * blocksize);

  vec<u32> blocks = blocks_for_inode(inode);

  char *dst = buf;

  for (auto b : blocks) {
    read_block(b, dst);
    dst += blocksize;
  }

  return buf;
}
*/

void fs::ext2::traverse_dir(u32 inode,
                            func<bool(fs::directory_entry)> callback) {
  TRACE;
  ext2_inode_info i;
  read_inode(i, inode);
  traverse_dir(i, callback);
}

void fs::ext2::traverse_dir(ext2_inode_info &inode,
                            func<bool(fs::directory_entry)> callback) {
  TRACE;
  // vec<u32> blocks = read_dir(inode);

  void *buffer = kmalloc(blocksize);
  traverse_blocks(blocks_for_inode(inode), buffer, [&](void *buffer) -> bool {
    auto *entry = reinterpret_cast<ext2_dir *>(buffer);
    while ((u64)entry < (u64)buffer + blocksize) {
      if (entry->inode != 0) {
        fs::directory_entry ent;
        ent.inode = entry->inode;
        for (u32 i = 0; i < entry->namelength; i++)
          ent.name.push(entry->name[i]);
        if (!callback(ent)) break;
      }
      entry = (ext2_dir *)((char *)entry + entry->size);
    }
    return true;
  });

  kfree(buffer);
}

vec<fs::directory_entry> fs::ext2::read_dir(u32 inode) {
  TRACE;
  ext2_inode_info info;
  read_inode(info, inode);
  return read_dir(info);
}

vec<fs::directory_entry> fs::ext2::read_dir(ext2_inode_info &inode) {
  TRACE;

  scoped_lock lck(m_lock);

  vec<fs::directory_entry> entries;

  traverse_dir(inode, [&](fs::directory_entry ent) -> bool {
    entries.push(move(ent));
    return true;
  });
  return entries;
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

static unique_ptr<fs::filesystem> ext2_mounter(ref<dev::device>, int flags) {
  printk("trying to mount\n");
  return nullptr;
}

static void ext2_init(void) { vfs::register_filesystem("ext2", ext2_mounter); }

module_init("fs::ext2", ext2_init);
