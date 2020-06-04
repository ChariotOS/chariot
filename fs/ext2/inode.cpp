#include <dev/driver.h>
#include <errno.h>
#include <fs.h>
#include <fs/ext2.h>
#include <util.h>

#define EXT2_ADDR_PER_BLOCK(node) (node->block_size / sizeof(u32))
#define EXT2_NDIR_BLOCKS 12
#define EXT2_IND_BLOCK EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS (EXT2_TIND_BLOCK + 1)


static int flush_info(fs::inode &ino);

static int ilog2(int x) {
  /*
   * Find the leftmost 1. Use a method that is similar to
   * binary search.
   */
  int result = 0;
  result = (!!(x >> 16)) << 4;  // if > 16?
  // based on previous result, if > (result + 8)
  result = result + ((!!(x >> (result + 8))) << 3);
  result = result + ((!!(x >> (result + 4))) << 2);
  result = result + ((!!(x >> (result + 2))) << 1);
  result = result + (!!(x >> (result + 1)));
  return result;
}

/**
 * block_to_path - given a block number, solve the indirect path to it
 *
 * @node: the inode you are looking into. This argument exists to get metadata,
 *        and no IO occurs
 * @block: the block index to find the path of
 * @offsets: the indirect path array to fill in
 * @boundary: an optional pointer to an integer that contains how many blocks
 *            remain in the last offset's block pointer
 */
static int block_to_path(fs::inode *node, int i_block, int offsets[4],
                         int *boundary = nullptr) {
  int ptrs = EXT2_ADDR_PER_BLOCK(node);
  int ptrs_bits = ilog2(ptrs);

  // how many block pointers are in each indirect block
  const long direct_blocks = 12;  // 12 direct blocks
  const long indirect_blocks = ptrs;
  const long double_blocks = (1 << (ptrs_bits * 2));

  int n = 0;
  int final = 0;

  if (i_block < 0) {
    printk("[EXT2 WARN] %s: block < 0\n", __func__);
  } else if (i_block < direct_blocks) {
    offsets[n++] = i_block;
    final = direct_blocks;
  } else if ((i_block -= direct_blocks) < indirect_blocks) {
    offsets[n++] = EXT2_IND_BLOCK;
    offsets[n++] = i_block;
    final = ptrs;
  } else if ((i_block -= indirect_blocks) < double_blocks) {
    offsets[n++] = EXT2_DIND_BLOCK;
    offsets[n++] = i_block >> ptrs_bits;
    offsets[n++] = i_block & (ptrs - 1);
    final = ptrs;
  } else if (((i_block -= double_blocks) >> (ptrs_bits * 2)) < ptrs) {
    offsets[n++] = EXT2_TIND_BLOCK;
    offsets[n++] = i_block >> (ptrs_bits * 2);
    offsets[n++] = (i_block >> ptrs_bits) & (ptrs - 1);
    offsets[n++] = i_block & (ptrs - 1);
    final = ptrs;
  } else {
    printk("[EXT2 WARN] %s: i_block is too big!\n", __func__);
  }

  if (boundary) *boundary = final - 1 - (i_block & (ptrs - 1));
  return n;
}

static struct fs::inode *ext2_inode(int type, u32 index, fs::ext2 &fs) {
  auto in = new fs::inode(type, fs);

  in->ino = index;

  return in;
}

int block_from_index(fs::inode &node, int i_block, int set_to = 0) {
  fs::ext2 *efs = static_cast<fs::ext2 *>(&node.sb);
  auto bsize = efs->block_size;

  auto p = node.priv<fs::ext2_idata>();
  // start the inodeS
  auto table = (int *)p->info.block_pointers;
  int path[4];
  int n = block_to_path(&node, i_block, path);

  for (int i = 0; i < n - 1; i++) {
    int off = path[i];
    if (p->blk_bufs[i] == NULL || p->cached_path[i] != off) {
      if (p->blk_bufs[i] == NULL) p->blk_bufs[i] = (int *)kmalloc(bsize);
      if (!efs->read_block(table[off], p->blk_bufs[i])) {
				printk("failef to read table[%d] = %d\n", off, table[off]);
        return 0;
      }
      p->cached_path[i] = off;
    }
    table = p->blk_bufs[i];
  }

  return table[path[n - 1]];
}




// I know, this is slow.
static vec<uint32_t> read_blocklist(fs::inode &ino,
                                    bool include_block_list_blocks = true) {
  fs::ext2 *efs = static_cast<fs::ext2 *>(&ino.sb);

  auto blocksize = efs->block_size;
  auto entries_per_block = blocksize / sizeof(uint32_t);

  auto block_count = ceil_div(ino.size, blocksize);
  auto blocks_remaining = block_count;
  vec<uint32_t> list;
  auto add_block = [&](uint32_t bi) {
    if (blocks_remaining) {
      list.push(bi);
      --blocks_remaining;
    }
  };


  if (include_block_list_blocks) {
    // This seems like an excessive over-estimate but w/e.
    list.ensure_capacity(blocks_remaining * 2);
  } else {
    list.ensure_capacity(blocks_remaining);
  }


	auto &e2inode = ino.priv<fs::ext2_idata>()->info;
  unsigned direct_count = min(block_count, (unsigned)EXT2_NDIR_BLOCKS);
  for (unsigned i = 0; i < direct_count; ++i) {
    auto block_index = e2inode.dbp[i];
    add_block(block_index);
  }

  if (!blocks_remaining) return list;


  auto process_block_array = [&](unsigned array_block_index, auto &&callback) {
    if (include_block_list_blocks) callback(array_block_index);
    auto array_block = new char[blocksize];
    efs->read_block(array_block_index, array_block);

    auto *array = reinterpret_cast<const uint32_t *>(array_block);
    unsigned count = min(blocks_remaining, entries_per_block);

    for (uint32_t i = 0; i < count; ++i) callback(array[i]);

    delete[] array_block;
  };


  process_block_array(e2inode.singly_block,
                      [&](unsigned block_index) { add_block(block_index); });


  if (!blocks_remaining) return list;

  process_block_array(e2inode.doubly_block, [&](unsigned block_index) {
    process_block_array(
        block_index, [&](unsigned block_index2) { add_block(block_index2); });
  });

  if (!blocks_remaining) return list;

  process_block_array(e2inode.triply_block, [&](unsigned block_index) {
    process_block_array(block_index, [&](unsigned block_index2) {
      process_block_array(block_index2, [&](unsigned block_index3) {
        add_block(block_index3);
      });
    });
  });

  return list;
}


// this is also really bad.
static int write_blocklist(fs::inode &ino, const vec<uint32_t> &blocklist) {
  /*
fs::ext2 *efs = static_cast<fs::ext2 *>(&ino.sb);

uint32_t blocksize = efs->block_size;
uint32_t entries_per_block = blocksize / sizeof(uint32_t);
// read the e2inode.
// TODO: cache me!
fs::ext2_inode_info e2inode;
efs->read_inode(e2inode, ino.ino);


// NOTE: There is a mismatch between i_blocks and blocks.size() since i_blocks
// includes meta blocks and blocks.size() does not.
auto old_block_count = ceil_div(e2inode.size, blocksize);
  */

  return 0;
}




// TODO: used to add a single block to the end of a file
static int append_block(fs::inode &ino, int dest) {
  fs::ext2 *efs = static_cast<fs::ext2 *>(&ino.sb);

	auto &info = ino.priv<fs::ext2_idata>()->info;

  auto blk = efs->balloc();
  if (blk == 0) return -ENOSPC;

  int path[4];
  int n = block_to_path(&ino, dest, path);


  // DEBUG
  printk("blocks[%d] = %d\n", dest, blk);
  // DEBUG

  if (n == 1) {
    // direct case
		printk("direct block %d <- %d", path[0], blk);
    info.dbp[path[0]] = blk;
    return 0;
  } else {
    return -ENOTIMPL;
  }


  return 0;
}

// TODO: remove a block from the end of a file, used for shrinking
static int pop_block(fs::inode &ino) { return 0; }

static int truncate(fs::inode &ino, size_t new_size) {
  auto old_size = ino.size;
  if (old_size == new_size) return 0;


  fs::ext2 *efs = static_cast<fs::ext2 *>(&ino.sb);

  auto block_size = efs->block_size;
  size_t bbefore = ceil_div(old_size, block_size);
  size_t bafter = ceil_div(new_size, block_size);

	if (new_size == ino.size) return 0;


  if (bafter > bbefore) {
    auto blocks_needed = bafter - bbefore;
		if (bafter > 12) {
			return -ENOTIMPL;
		}
    printk("need to allocate %d on the end\n", blocks_needed);

    if (efs->sb->unallocatedblocks < blocks_needed) return -ENOSPC;

    for (int i = 0; i < blocks_needed; i++) {
      // push the blocks on the end :)
      int err = append_block(ino, bbefore + i);
      if (err < 0) {
        // not sure how to recover just yet, as we already handle the case of
        // ENOSPC up above...
        panic("append_block failed with error %d\n", -err);
      }
    }



  } else if (bafter < bbefore) {
    auto blocks_to_remove = bbefore - bafter;
    printk("need to remove %d\n", blocks_to_remove);
    return -ENOTIMPL;
  }

  // :^)
  ino.size = new_size;
  // printk("resize to %d\n", new_size);

  flush_info(ino);


	/*
	// DEBUG
	auto olist = read_blocklist(ino);
	printk("olist for ino %d: [", ino.ino);
	for (int i = 0; i < olist.size(); i++) {
		printk("%d", olist[i]);
		if (i != olist.size() - 1) printk(", ");
	}
	printk("]\n");
	// DEBUG
	*/

  return 0;
}


// returns the number of bytes read or negative values on failure
static ssize_t ext2_raw_rw(fs::inode &ino, char *buf, size_t sz, off_t offset,
                           bool write) {
  fs::ext2 *efs = static_cast<fs::ext2 *>(&ino.sb);
  if (write) {
    off_t total_needed = offset + sz;
    if (ino.size < total_needed) {
      truncate(ino, total_needed);
    }

    /*
    auto olist = read_blocklist(ino);

    printk("blocklist for ino %d: [", ino.ino);
    for (int i = 0; i < olist.size(); i++) {
            printk("%d", olist[i]);
            if (i != olist.size() - 1) printk(", ");
    }
    printk("]\n");
    */

    // TODO: ensure blocks are avail
  }


  // if (offset > ino.size) return 0;

  // how many bytes have been read
  ssize_t nread = 0;

  // the size of a single block
  auto bsize = efs->block_size;

  off_t first_blk_ind = offset / bsize;
  off_t last_blk_ind = (offset + sz) / bsize;
  off_t offset_into_first_block = offset % bsize;

  // TODO: lock the FS. We now want to own the efs.work_buf
  auto *given_buf = (u8 *)buf;

  int remaining_count = min((off_t)sz, (off_t)ino.size - offset);

  for (int bi = first_blk_ind; remaining_count && bi <= last_blk_ind; bi++) {
    u32 blk = block_from_index(ino, bi);
    if (blk == 0) {
      printk("ext2fs: read_bytes: failed at lbi %u\n", bi);
      return -EIO;
    }

    auto *buf = (u8 *)efs->work_buf;
    efs->read_block(blk, buf);

    int offset_into_block = (bi == first_blk_ind) ? offset_into_first_block : 0;
    int num_bytes_to_copy = min(bsize - offset_into_block, remaining_count);

    if (write) {
      memcpy(buf + offset_into_block, given_buf, num_bytes_to_copy);
      efs->write_block(blk, buf);
    } else {
      memcpy(given_buf, buf + offset_into_block, num_bytes_to_copy);
    }
    remaining_count -= num_bytes_to_copy;
    nread += num_bytes_to_copy;
    given_buf += num_bytes_to_copy;
  }
  return nread;
}

typedef struct __ext2_dir_entry {
  uint32_t inode;
  uint16_t size;
  uint8_t namelength;
  uint8_t reserved;
  char name[0];
  /* name here */
} __attribute__((packed)) ext2_dir;

static void ext2_traverse_dir(fs::inode &ino,
                              func<void(u32 ino, const char *name)> fn) {
  auto *ents = (ext2_dir *)kmalloc(ino.size);

  // read the entire file into the buffer
  int res = ext2_raw_rw(ino, (char *)ents, ino.size, 0, false);
  if (res < 0) {
    panic("ext2: failed to read directory contents. Error code %d\n", res);
  }

  char namebuf[255];
  for (auto *entry = ents; (size_t)entry < (size_t)ents + ino.size;
       entry = (ext2_dir *)((char *)entry + entry->size)) {
    if (entry->inode != 0) {
      memcpy(namebuf, entry->name, entry->namelength);
      namebuf[entry->namelength] = 0;
      fn(entry->inode, namebuf);
    }
  }

  kfree(ents);
}

static int flush_info(fs::inode &ino) {
  fs::ext2 *efs = static_cast<fs::ext2 *>(&ino.sb);
	auto &info = ino.priv<fs::ext2_idata>()->info;

  info.size = ino.size;
  info.type = ino.mode;
  info.uid = ino.uid;
  info.gid = ino.gid;

  info.hardlinks = ino.link_count;
  info.last_access = ino.atime;
  info.create_time = ino.ctime;
  info.delete_time = ino.dtime;
	// TODO: update all this stuff too
  efs->write_inode(info, ino.ino);
  return 0;
}




static int injest_info(fs::inode &ino, fs::ext2_inode_info &info) {
  fs::ext2 *efs = static_cast<fs::ext2 *>(&ino.sb);
  ino.size = info.size;
  ino.mode = info.type;
  ino.uid = info.uid;
  ino.gid = info.gid;
  ino.link_count = info.hardlinks;
  ino.atime = info.last_access;
  ino.ctime = info.create_time;
  ino.mtime = info.last_modif;
  ino.dtime = info.delete_time;

  ino.block_size = efs->block_size;

  auto table = (u32 *)info.dbp;
  // copy the block pointers
  for (int i = 0; i < 15; i++) {
    ino.priv<fs::ext2_idata>()->info.block_pointers[i] = table[i];
  }

  if (ino.type == T_DIR) {
    // TODO: clear out the dirents
    ext2_traverse_dir(ino, [&](u32 nr, const char *name) {
      ino.register_direntry(name, ENT_RES, nr);
    });
  } else if (ino.type == T_CHAR || ino.type == T_BLK) {
    // parse the major/minor
    unsigned dev = info.dbp[0];
    if (!dev) dev = info.dbp[1];
    ino.major = (dev & 0xfff00) >> 8;
    ino.minor = (dev & 0xff) | ((dev >> 12) & 0xfff00);
  }
  return 0;
}

#define UNIMPL() printk("[ext2] '%s' NOT IMPLEMENTED\n", __PRETTY_FUNCTION__)

static int ext2_seek(fs::file &, off_t, off_t) {
  return 0;  // allow seek
}

static ssize_t ext2_do_read_write(fs::file &f, char *buf, size_t nbytes,
                                  bool is_write) {
  ssize_t nread = ext2_raw_rw(*f.ino, buf, nbytes, f.offset(), is_write);
  if (nread >= 0) {
    f.seek(nread, SEEK_CUR);
  }
  return nread;
}

static ssize_t ext2_read(fs::file &f, char *dst, size_t sz) {
  if (f.ino->type != T_FILE) return -EINVAL;
  return ext2_do_read_write(f, dst, sz, false);
}

static ssize_t ext2_write(fs::file &f, const char *src, size_t sz) {
  if (f.ino->type != T_FILE) return -EINVAL;
  return ext2_do_read_write(f, (char *)src, sz, true);
}

static int ext2_ioctl(fs::file &, unsigned int, off_t) {
  UNIMPL();
  return -ENOTIMPL;
}

static int ext2_open(fs::file &) { return 0; }
static void ext2_close(fs::file &) {}

static int ext2_mmap(fs::file &, struct mm::area &) {
  UNIMPL();
  return -ENOTIMPL;
}

static int ext2_resize(fs::file &, size_t) {
  UNIMPL();
  return -ENOTIMPL;
}

fs::ext2_idata::~ext2_idata() {
  for (int i = 0; i < 4; i++) {
    if (blk_bufs[i] != nullptr) {
      kfree(blk_bufs[i]);
    }
  }
}

static void ext2_destroy_priv(fs::inode &v) { delete v.priv<fs::ext2_idata>(); }

fs::file_operations ext2_file_ops{
    .seek = ext2_seek,
    .read = ext2_read,
    .write = ext2_write,
    .ioctl = ext2_ioctl,
    .open = ext2_open,
    .close = ext2_close,
    .mmap = ext2_mmap,
    .resize = ext2_resize,
    .destroy = ext2_destroy_priv,
};

static int ext2_create(fs::inode &node, const char *name,
                       struct fs::file_ownership &) {
  fs::ext2 *efs = static_cast<fs::ext2 *>(&node.sb);
  int ino = efs->allocate_inode();
  printk("ino=%d\n", ino);

  return -ENOTIMPL;
}

static int ext2_mkdir(fs::inode &, const char *name,
                      struct fs::file_ownership &) {
  UNIMPL();
  return -ENOTIMPL;
}

static int ext2_unlink(fs::inode &, const char *) {
  UNIMPL();
  return -ENOTIMPL;
}

static struct fs::inode *ext2_lookup(fs::inode &node, const char *needle) {
  if (node.type != T_DIR) panic("ext2_lookup on non-dir\n");

  bool found = false;
  auto efs = static_cast<fs::ext2 *>(&node.sb);
  int nr = -1;

  // walk the linked list to get the inode num
  for (auto *it = node.dir.entries; it != NULL; it = it->next) {
    if (!strcmp(it->name.get(), needle)) {
      if (it->mount_shadow != NULL) {
        printk("here\n");
        return it->mount_shadow;
      }
      nr = it->nr;
      found = true;
      break;
    }
  }

  if (found) {
    return efs->get_inode(nr);
  }
  return NULL;
}

static int ext2_mknod(fs::inode &, const char *name,
                      struct fs::file_ownership &, int major, int minor) {
  UNIMPL();
  return -ENOTIMPL;
}


fs::dir_operations ext2_dir_ops{
    .create = ext2_create,
    .mkdir = ext2_mkdir,
    .unlink = ext2_unlink,
    .lookup = ext2_lookup,
    .mknod = ext2_mknod,
};

struct fs::inode *fs::ext2::create_inode(ext2 *fs, u32 index) {
  fs::inode *ino = NULL;

  struct ext2_inode_info info;

  fs->read_inode(info, index);

  int ino_type = T_INVA;

  auto type = ((info.type) & 0xF000) >> 12;

  if (type == 0x1) ino_type = T_FIFO;
  if (type == 0x2) ino_type = T_CHAR;
  if (type == 0x4) ino_type = T_DIR;
  if (type == 0x6) ino_type = T_BLK;
  if (type == 0x8) ino_type = T_FILE;
  if (type == 0xA) ino_type = T_SYML;
  if (type == 0xC) ino_type = T_SOCK;

  ino = ext2_inode(ino_type, index, *fs);
  ino->priv<ext2_idata>() = new ext2_idata();

  ino->dev.major = fs->disk->ino->major;
  ino->dev.minor = fs->disk->ino->minor;
  ino->fops = &ext2_file_ops;
  ino->dops = &ext2_dir_ops;

  injest_info(*ino, info);

  if (ino_type == T_CHAR || ino_type == T_BLK) {
    dev::populate_inode_device(*ino);
  }

  return ino;
}
