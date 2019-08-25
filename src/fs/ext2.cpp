#include <asm.h>
#include <dev/blk_dev.h>
#include <errno.h>
#include <fs/ext2.h>
#include <mem.h>
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

struct [[gnu::packed]] fs::ext2::superblock {
  uint32_t inodes;
  uint32_t blocks;
  uint32_t reserved_for_root;
  uint32_t unallocatedblocks;
  uint32_t unallocatedinodes;
  uint32_t superblock_id;
  uint32_t blocksize_hint;     // shift by 1024 to the left
  uint32_t fragmentsize_hint;  // shift by 1024 to left
  uint32_t blocks_in_blockgroup;
  uint32_t frags_in_blockgroup;
  uint32_t inodes_in_blockgroup;
  uint32_t last_mount;
  uint32_t last_write;
  uint16_t mounts_since_last_check;
  uint16_t max_mounts_since_last_check;
  uint16_t ext2_sig;  // 0xEF53
  uint16_t state;
  uint16_t op_on_err;
  uint16_t minor_version;
  uint32_t last_check;
  uint32_t max_time_in_checks;
  uint32_t os_id;
  uint32_t major_version;
  uint16_t uuid;
  uint16_t gid;

  u32 s_first_ino;              /* First non-reserved inode */
  u16 s_inode_size;             /* size of inode structure */
  u16 s_block_group_nr;         /* block group # of this superblock */
  u32 s_feature_compat;         /* compatible feature set */
  u32 s_feature_incompat;       /* incompatible feature set */
  u32 s_feature_ro_compat;      /* readonly-compatible feature set */
  u8 s_uuid[16];                /* 128-bit uuid for volume */
  char s_volume_name[16];       /* volume name */
  char s_last_mounted[64];      /* directory where last mounted */
  u32 s_algorithm_usage_bitmap; /* For compression */
  /*
   * Performance hints.  Directory preallocation should only
   * happen if the EXT2_FEATURE_COMPAT_DIR_PREALLOC flag is on.
   */
  u8 s_prealloc_blocks;      /* Nr of blocks to try to preallocate*/
  u8 s_prealloc_dir_blocks;  /* Nr to preallocate for dirs */
  u16 s_reserved_gdt_blocks; /* Per group table for online growth */
  /*
   * Journaling support valid if EXT2_FEATURE_COMPAT_HAS_JOURNAL set.
   */
  u8 s_journal_uuid[16]; /* uuid of journal superblock */
  u32 s_journal_inum;    /* inode number of journal file */
  u32 s_journal_dev;     /* device number of journal file */
  u32 s_last_orphan;     /* start of list of inodes to delete */
  u32 s_hash_seed[4];    /* HTREE hash seed */
  u8 s_def_hash_version; /* Default hash version to use */
  u8 s_jnl_backup_type;  /* Default type of journal backup */
  u16 s_desc_size;       /* Group desc. size: INCOMPAT_64BIT */
  u32 s_default_mount_opts;
  u32 s_first_meta_bg;      /* First metablock group */
  u32 s_mkfs_time;          /* When the filesystem was created */
  u32 s_jnl_blocks[17];     /* Backup of the journal inode */
  u32 s_blocks_count_hi;    /* Blocks count high 32bits */
  u32 s_r_blocks_count_hi;  /* Reserved blocks count high 32 bits*/
  u32 s_free_blocks_hi;     /* Free blocks count */
  u16 s_min_extra_isize;    /* All inodes have at least # bytes */
  u16 s_want_extra_isize;   /* New inodes should reserve # bytes */
  u32 s_flags;              /* Miscellaneous flags */
  u16 s_raid_stride;        /* RAID stride */
  u16 s_mmp_interval;       /* # seconds to wait in MMP checking */
  u64 s_mmp_block;          /* Block for multi-mount protection */
  u32 s_raid_stripe_width;  /* blocks on all data disks (N*stride)*/
  u8 s_log_groups_per_flex; /* FLEX_BG group size */
  u8 s_reserved_char_pad;
  u16 s_reserved_pad;  /* Padding to next 32bits */
  u32 s_reserved[162]; /* Padding to the end of the block */
};

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

fs::ext2::ext2(dev::blk_dev &dev) : filesystem(/*super*/), dev(dev) { TRACE; }

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
  bool res = dev.read(1024, 1024, sb);

  if (!res) {
    printk("failed to read the superblock\n");
    return false;
  }

  // first, we need to make 100% sure this is actually a disk with ext2 on it...
  if (sb->ext2_sig != 0xef53) {
    printk("Block device does not contain the ext2 signature\n");
    return false;
  }

  // solve for the filesystems block size
  blocksize = 1024 << sb->blocksize_hint;

  // the number of block groups can be found by rounding up the
  // blocks/blocks_per_group
  blockgroups = ceil((double)sb->blocks / (double)sb->blocks_in_blockgroup);

  first_bgd = sb->superblock_id + (sizeof(fs::ext2::superblock) / blocksize);

  // allocate a block for the work_buf
  work_buf = kmalloc(blocksize);
  inode_buf = kmalloc(blocksize);

  m_root_inode = get_inode(2);

  return true;
}

bool fs::ext2::read_inode(ext2_inode_info &dst, u32 inode) {
  TRACE;
  return read_inode(&dst, inode);
}

bool fs::ext2::read_inode(ext2_inode_info *dst, u32 inode) {
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

  memcpy(dst, _inode, sizeof(ext2_inode_info));

  return true;
}

bool fs::ext2::read_inode(ext2_inode *dst, u32 inode) {
  TRACE;
  return read_inode(&dst->info, inode);
}

/**
 *
 *
 *
 */

bool fs::ext2::write_inode(ext2_inode_info &src, u32 inode) {
  TRACE;
  return read_inode(&src, inode);
}

bool fs::ext2::write_inode(ext2_inode *src, u32 inode) {
  TRACE;
  return read_inode(&src->info, inode);
}

bool fs::ext2::write_inode(ext2_inode_info *src, u32 inode) {
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
  memcpy(_inode, src, sizeof(ext2_inode_info));

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
  return dev.read(block * blocksize, blocksize, buf);
}

bool fs::ext2::write_block(u32 block, const void *buf) {
  TRACE;
  return dev.write(block * blocksize, blocksize, buf);
}

void fs::ext2::traverse_blocks(vec<u32> blks, void *buf,
                               func<bool(void *)> callback) {
  TRACE;
  for (auto b : blks) {
    read_block(b, buf);
    if (!callback(buf)) return;
  }
}

void *fs::ext2::read_entire(ext2_inode_info &inode) {
  TRACE;
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
    return true; });

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
  vec<fs::directory_entry> entries;

  traverse_dir(inode, [&](fs::directory_entry ent) -> bool {
    entries.push(move(ent));
    return true;
  });
  return entries;
}

int fs::ext2::read_file(u32 inode, u32 off, u32 len, u8 *buf) {
  TRACE;
  ext2_inode_info info;
  read_inode(info, inode);

  return read_file(info, off, len, buf);
}

int fs::ext2::read_file(ext2_inode_info &inode, u32 off, u32 len, u8 *buf) {
  TRACE;
  return 0;
}

fs::vnoderef fs::ext2::get_root_inode(void) { return m_root_inode; }

// TODO
fs::vnoderef fs::ext2::get_inode(u32 index) {
  TRACE;
  auto in = make_ref<fs::ext2_inode>(*this, index);
  read_inode(in->info, index);
  return in;
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

fs::vnoderef fs::ext2::open(fs::path path, u32 flags) {
  TRACE;
  if (!path.is_root()) {
    printk("ext2::open requires absolute paths (\"%s\")\n",
           path.to_string().get());
    return {};
  }

  INFO("looking for %s\n", path.to_string().get());

  // starting at the root node (always 2)
  u32 inode = 2;
  ext2_inode_info info;

  for (int i = 0; i < path.len(); i++) {
    // read the node's info
    read_inode(info, inode);
    const string &name = path[i];

    INFO("============================\n");
    INFO("searching %d for %s\n", inode, name.get());
    INFO("\n");

    u16 type = info.type & 0xF000;

    printk("type=%d\n", type);

    INFO("type=%04x\n", type);

    if (type == 0x4000 /*directory*/) {
      INFO("DIR\n");
      auto contents = read_dir(info);
      bool found = false;

      for (auto &file : contents) {
        if (file.name == name) {
          INFO("%s@%d\n", file.name.get(), file.inode);
          inode = file.inode;
          found = true;
          break;
        }
      }

      if (!found) {
        return {};
      }

    } else if (type == 0x8000 /*regular file*/) {
      INFO("FILE\n");
      INFO("regular file!\n");
      return {};
    }
  }

  // check if the file is a symlink

  // INFO("============================\n");
  // construct the inode object and return it
  auto res = make_ref<fs::ext2_inode>(*this, inode);
  res->info = info;

  return res;
}

fs::ext2_inode::ext2_inode(fs::ext2 &fs, u32 index) : vnode(fs, index) {
  TRACE;
}

fs::ext2_inode::~ext2_inode() {
  TRACE;
  // TODO
}

bool fs::ext2_inode::walk_dir_impl(func<bool(const string &, ref<vnode>)> &cb) {
  auto *efs = static_cast<ext2 *>(&fs());

  efs->traverse_dir(this->info, [&](fs::directory_entry de) -> bool {
    return cb(de.name, efs->get_inode(de.inode));
  });
  return false;
}

int fs::ext2_inode::add_dir_entry(ref<vnode> node, const string &name,
                                  u32 mode) {
  return -ENOTIMPL;
}

fs::inode_metadata fs::ext2_inode::metadata(void) {
  fs::inode_metadata md;

  md.size = info.size;
  md.mode = info.type & 0xFFF;

  auto type = ((info.type) & 0xF000) >> 12;

  md.type = inode_type::unknown;
  if (type == 0x1) md.type = inode_type::fifo;
  if (type == 0x2) md.type = inode_type::char_dev;
  if (type == 0x4) md.type = inode_type::dir;
  if (type == 0x6) md.type = inode_type::block_dev;
  if (type == 0x8) md.type = inode_type::file;
  if (type == 0xA) md.type = inode_type::symlink;
  if (type == 0xC) md.type = inode_type::unix_socket;

  md.mode = info.type & 0xFFF;

  md.uid = info.uid;
  md.gid = info.gid;
  md.link_count = info.hardlinks;

  md.atime = info.last_access;
  md.ctime = info.create_time;
  md.mtime = info.last_modif;
  md.dtime = info.delete_time;
  md.block_size = fs().block_size();

  md.block_count = md.size / md.block_size;
  if (md.size % md.block_size != 0) md.block_count++;

  return md;
}

off_t fs::ext2_inode::block_for_byte(off_t b) {
  auto *efs = static_cast<ext2 *>(&fs());

  auto blksz = efs->block_size();

  off_t blk_ind = b % blksz;

  printk("blk_ind = %d\n", blk_ind);

  off_t block = 0;

  if (blk_ind < 12) {
    block = info.dbp[blk_ind];
  } else {
    blk_ind = -ENOTIMPL;
  }

  // info.singly_block[blk_ind];

  return block;
}

ssize_t fs::ext2_inode::read(off_t off, size_t nbytes, void *dst) {
  if (is_dir()) return -EISDIR;


  auto *efs = static_cast<ext2 *>(&fs());

  // TODO cache the blocks somewhere
  auto blks = efs->blocks_for_inode(index());

  if (blks.size() == 0) return 0;

  ssize_t offset = off;
  if (offset > info.size) return 0;

  // how many bytes have been read
  ssize_t nread = 0;

  auto bsize = efs->block_size();

  off_t first_blk_ind = offset / bsize;
  off_t last_blk_ind = (offset + nbytes) / bsize;

  off_t offset_into_first_block = offset % bsize;

  // TODO: lock the FS. We now want to own the efs->work_buf
  auto *out = (u8 *)dst;

  int remaining_count = min((off_t)nbytes, (off_t)size() - off);

  for (int bi = first_blk_ind; remaining_count && bi <= last_blk_ind; ++bi) {
    auto *buf = (u8 *)efs->work_buf;

    if (!efs->read_block(blks[bi], buf)) {
      printk("ext2fs: read_bytes: read_block(%u) failed (lbi: %u)\n", blks[bi],
             bi);
      return -EIO;
    }

    int offset_into_block = (bi == first_blk_ind) ? offset_into_first_block : 0;
    int num_bytes_to_copy = min(bsize - offset_into_block, remaining_count);
    memcpy(out, buf + offset_into_block, num_bytes_to_copy);
    remaining_count -= num_bytes_to_copy;
    nread += num_bytes_to_copy;
    out += num_bytes_to_copy;
  }

  return nread;
}
