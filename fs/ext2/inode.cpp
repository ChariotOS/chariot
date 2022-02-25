#include <dev/driver.h>
#include <errno.h>
#include <fs.h>
#include <fs/ext2.h>
#include <mm.h>
#include <util.h>

#define EXT2_ADDR_PER_BLOCK(node) (node->block_size() / sizeof(u32))
#define EXT2_NDIR_BLOCKS 12
#define EXT2_IND_BLOCK EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS (EXT2_TIND_BLOCK + 1)


ext2::Node &downcast(fs::Node &n) { return *(ext2::Node *)(&n); }


static int flush_info(fs::Node &ino);

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
static int block_to_path(ck::ref<fs::Node> node, int i_block, int offsets[4], int *boundary = nullptr) {
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



int block_from_index(fs::Node &_n, int i_block, int set_to = 0) {
  auto &node = downcast(_n);
  ext2::FileSystem *efs = static_cast<ext2::FileSystem *>(node.sb.get());
  auto bsize = efs->block_size;

  // auto p = node.priv<ext2::ext2_idata>();
  // start the inodeS
  auto table = (int *)node.info.block_pointers;
  int path[4];
  int n = block_to_path(&node, i_block, path);
  scoped_lock l(node.path_cache_lock);

  for (int i = 0; i < n - 1; i++) {
    int off = path[i];
    if (node.blk_bufs[i] == NULL || node.cached_path[i] != off) {
      if (node.blk_bufs[i] == NULL) node.blk_bufs[i] = (int *)malloc(bsize);
      if (!efs->read_block(table[off], node.blk_bufs[i])) {
        return -EIO;
      }
      node.cached_path[i] = off;
    }
    table = node.blk_bufs[i];
  }

  return table[path[n - 1]];
}

static int access_block_path(fs::Node &ino, int n, int *path, ck::func<bool(uint32_t &)> cb) {
  ext2::FileSystem *efs = static_cast<ext2::FileSystem *>(ino.sb.get());
  auto &node = downcast(ino);
  auto &info = node.info;
  uint32_t blocksize = efs->block_size;


  // in the case of direct blocks, go through the short path.
  if (n == 1) {
    auto *blks = info.dbp;
    cb(blks[path[0]]);  // we dont care about if we wrote
    return 0;
  }

  // lock the path logic
  scoped_lock l(node.path_cache_lock);

  // make sure that the first index is valid as to avoid duplication of work
  if (info.block_pointers[path[0]] == 0) {
    info.block_pointers[path[0]] = efs->balloc();
    assert(info.block_pointers[path[0]]);
  }

  // TODO: lock the path nonsense
  // make sure the path bits are setup right.
  for (int i = 0; i < n; i++) {
    int off = path[i];
    if (node.blk_bufs[i] == NULL || node.cached_path[i] != off) {
      if (node.blk_bufs[i] == NULL) node.blk_bufs[i] = (int *)malloc(blocksize);
      node.cached_path[i] = off;
    }
  }

  // buffers for each level of indirection
  auto single_block = (uint32_t *)node.blk_bufs[0];
  auto double_block = (uint32_t *)node.blk_bufs[1];
  auto triple_block = (uint32_t *)node.blk_bufs[2];

  // block numbers for each of the levels
  int singly = -1;
  int doubly = -1;
  int triply = -1;

  int single_index = path[0];
  int double_index = path[1];
  int triple_index = path[2];

  singly = info.block_pointers[path[0]];
  if (node.cached_path[0] != single_index) {
    efs->read_block(singly, single_block);
    node.cached_path[0] = single_index;
  }


  // make sure there is a block
  if (single_block[double_index] == 0) {
    if (cb(single_block[double_index])) efs->write_block(singly, single_block);
  }

  // Early return
  if (n == 2) return 0;

  // now for double indirection
  doubly = single_block[double_index];
  if (node.cached_path[1] != double_index) {
    efs->read_block(doubly, double_block);
    node.cached_path[1] = double_index;
  }


  // make sure there is a block
  if (double_block[triple_index] == 0) {
    if (cb(double_block[triple_index])) efs->write_block(doubly, double_block);
  }


  // Early return
  if (n == 3) return 0;

  // now for triple indirection
  triply = double_block[triple_index];
  if (node.cached_path[2] != triple_index) {
    efs->read_block(triply, triple_block);
    node.cached_path[2] = triple_index;
  }


  // make sure there is a block
  if (triple_block[path[3]] == 0) {
    triple_block[path[3]] = efs->balloc();
    if (cb(triple_block[path[3]])) efs->write_block(triply, triple_block);
  }


  // Early return
  if (n == 4) return 0;

  panic("Oof. Invalid indirection of n=%d\n", n);

  return -EIO;
}


// TODO: used to add a single block to the end of a file
static int append_block(fs::Node &ino, int dest) {
  int path[4];
  memset(path, 0, 4 * 4);
  int n = block_to_path(&ino, dest, path);


  ext2::FileSystem *efs = static_cast<ext2::FileSystem *>(ino.sb.get());

  return access_block_path(ino, n, path, [&](uint32_t &dst) -> bool {
    int blk = efs->balloc();
    if (dst) printk("%d becomes %d\n", dst, blk);
    dst = blk;
    // we wrote
    return true;
  });
}




// TODO: remove a block from the end of a file, used for shrinking
static int pop_block(fs::Node &ino) { return 0; }

static int truncate(fs::Node &node, size_t new_size) {
  auto &ino = downcast(node);
  auto old_size = ino.size();
  if (old_size == new_size) return 0;


  ext2::FileSystem *efs = static_cast<ext2::FileSystem *>(ino.sb.get());

  auto block_size = efs->block_size;
  size_t bbefore = ceil_div(old_size, block_size);
  size_t bafter = ceil_div(new_size, block_size);

  if (new_size == ino.size()) return 0;


  if (bafter > bbefore) {
    auto blocks_needed = bafter - bbefore;

    // check (with extra space) that there is enough space left on the disk
    if ((efs->sb->unallocatedblocks - 32) < blocks_needed) return -ENOSPC;

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
    // auto blocks_to_remove = bbefore - bafter;
    // printk("need to remove %d\n", blocks_to_remove);
    pop_block(ino);
    return -ENOTIMPL;
  }

  // :^)
  ino.set_size(new_size);
  // printk("resize to %d\n", new_size);
  flush_info(ino);

  return 0;
}


// returns the number of bytes read or negative values on failure
static ssize_t ext2_raw_rw(fs::Node &node, char *buf, size_t sz, off_t offset, bool write) {
  auto &ino = downcast(node);
  ext2::FileSystem *efs = static_cast<ext2::FileSystem *>(ino.sb.get());

  if (write) {
    off_t total_needed = offset + sz;
    if (ino.size() < total_needed) {
      int tres = truncate(ino, total_needed);
      // propagate the error if there was one...
      if (tres < 0) return tres;
    }
  }


  // how many bytes have been read
  ssize_t nread = 0;

  // the size of a single block
  auto bsize = efs->block_size;

  off_t first_blk_ind = offset / bsize;
  off_t last_blk_ind = (offset + sz) / bsize;
  off_t offset_into_first_block = offset % bsize;

  // TODO: lock the FS. We now want to own the efs.work_buf
  auto *given_buf = (u8 *)buf;

  int remaining_count = min((off_t)sz, (off_t)ino.size() - offset);

  for (int bi = first_blk_ind; remaining_count && bi <= last_blk_ind; bi++) {
    u32 blk = block_from_index(ino, bi);
    if (blk == 0) {
      int path[4];
      memset(path, 0, 4 * 4);
      int n = block_to_path(&ino, bi, path);

      printk("path: [");
      for (int i = 0; i < n; i++) {
        printk("%d", path[i]);
        if (i != n - 1) printk(", ");
      }
      printk("]\n");
      panic("ext2fs: invalid block allocated at lbi %u\n", bi);
      return -EIO;
    }

    int offset_into_block = (bi == first_blk_ind) ? offset_into_first_block : 0;
    int num_bytes_to_copy = min(bsize - offset_into_block, remaining_count);


    // load the buffer from the buffer cache
    auto buf_bb = bref::get(*efs->bdev, blk);
    auto *buf = (u8 *)buf_bb->data();

    if (write) {
      // write to the buffer
      memcpy(buf + offset_into_block, given_buf, num_bytes_to_copy);
      buf_bb->register_write();
    } else {
      // read from the buffer
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

static void ext2_traverse_dir(fs::Node &ino, ck::func<void(u32 ino, const char *name)> fn) {
  auto *ents = (ext2_dir *)malloc(ino.size());

  // read the entire file into the buffer
  int res = ext2_raw_rw(ino, (char *)ents, ino.size(), 0, false);
  if (res < 0) {
    panic("ext2: failed to read directory contents. Error code %d\n", res);
  }

  char namebuf[255];
  for (auto *entry = ents; (size_t)entry < (size_t)ents + ino.size(); entry = (ext2_dir *)((char *)entry + entry->size)) {
    if (entry->inode != 0) {
      memcpy(namebuf, entry->name, entry->namelength);
      namebuf[entry->namelength] = 0;
      fn(entry->inode, namebuf);
    }
  }

  free(ents);
}

static int flush_info(fs::Node &node) {
  auto &ino = downcast(node);
  ext2::FileSystem *efs = static_cast<ext2::FileSystem *>(ino.sb.get());
  auto &info = ino.info;

  info.size = ino.size();

  auto blocks = ceil_div((uint32_t)info.size, (uint32_t)efs->block_size);
  auto sectors_in_block = efs->block_size / 512;
  info.disk_sectors = blocks * sectors_in_block;

  // printk("size: %d, blocks: %d, sectors: %d\n", info.size, blocks,
  // info.disk_sectors);

  info.disk_sectors /= efs->sector_size;
  if (ino.is_dir()) {
    info.type = EXT2_FT_DIR;
  } else if (ino.is_file()) {
    info.type = EXT2_FT_REG_FILE;
  } else {
    KWARN("ext2: unknown file type for node. Writing EXT2_FT_UNKNOWN.\n");
    info.type = EXT2_FT_UNKNOWN;
  }
  info.uid = ino.uid();
  info.gid = ino.gid();

  info.hardlinks = ino.nlink();
  info.last_access = ino.metadata().accessed_time;
  info.create_time = ino.metadata().create_time;
  info.delete_time = 0;  // ?
  // TODO: update all this stuff too
  efs->write_inode(info, ino.ino);
  return 0;
}




static int injest_info(fs::Node &node, ext2::ext2_inode_info &info) {
  auto &ino = downcast(node);
  ext2::FileSystem *efs = static_cast<ext2::FileSystem *>(ino.sb.get());

  ino.set_size(info.size);
  ino.set_mode(info.type);
  ino.set_uid(info.uid);
  ino.set_gid(info.gid);
  ino.set_nlink(info.hardlinks);
  ino.set_accessed_time(info.last_access);
  ino.set_create_time(info.create_time);
  ino.set_modified_time(info.last_modif);
  // ino.set_dtime(info.delete_time);

  ino.set_block_size(efs->block_size);

  auto table = (u32 *)info.dbp;
  // copy the block pointers
  for (int i = 0; i < 15; i++) {
    ino.info.block_pointers[i] = table[i];
  }
  return 0;
}


struct ext2_vmobject final : public mm::VMObject {
  ext2_vmobject(ck::ref<fs::Node> ino, size_t npages, off_t off) : VMObject(npages) {
    m_ino = ino;
    m_off = off;
  }

  virtual ~ext2_vmobject(void) {
    for (auto &kv : buffers) {
      bput(kv.value);
    }
  };


  struct block::Buffer *bget(uint32_t n) {
    scoped_lock l(m_lock);

    struct block::Buffer *buf = buffers[n];

    if (buf == NULL) {
      ext2::FileSystem *efs = static_cast<ext2::FileSystem *>(m_ino->sb.get());

      auto i_block = (m_off / efs->block_size) + ((n * PGSIZE) / efs->block_size);
      int block = block_from_index(*m_ino, i_block);

      // we hold the buffer hostage and don't release it back till this region
      // is destructed
      buf = efs->bget(block);
      buffers[n] = buf;
    }

    return buf;
  }

  // get a shared page (page #n in the mapping)
  virtual ck::ref<mm::Page> get_shared(off_t n) override {
    // printk("get_shared(%d)\n", n);
    auto blk = bget(n);

    // hexdump(blk->data(), PGSIZE, true);

    return blk->page();
  }

  virtual void flush(off_t n) override {
    auto blk = bget(n);
    // TODO: do this in a more async way (hand off to syncd or something)
    blk->flush();
  }


 private:
  // offset -> block
  spinlock m_lock;
  ck::map<uint32_t, block::Buffer *> buffers;
  ck::ref<fs::Node> m_ino;
  off_t m_off = 0;
};




int ext2::FileNode::seek(fs::File &, off_t old_off, off_t new_off) {
  EXT_DEBUG("seek, %zu %zu\n", old_off, new_off);
  return 0;  // allow seek
}

ssize_t ext2::FileNode::read(fs::File &f, char *buf, size_t count) {
  EXT_DEBUG("read %p %zu\n", buf, count);
  ssize_t nread = ext2_raw_rw(*this, buf, count, f.offset(), false);
  if (nread >= 0) {
    f.seek(nread, SEEK_CUR);
  }
  return nread;
}
ssize_t ext2::FileNode::write(fs::File &f, const char *buf, size_t count) {
  EXT_DEBUG("write %p %zu\n", buf, count);
  ssize_t nwrite = ext2_raw_rw(*this, (char *)buf, count, f.offset(), true);
  if (nwrite >= 0) {
    f.seek(nwrite, SEEK_CUR);
  }
  return nwrite;
}

int ext2::FileNode::resize(fs::File &, size_t) {
  UNIMPL();
  return -ENOTIMPL;
}

ssize_t ext2::FileNode::size(void) {
  UNIMPL();
  return 0;
}

ext2::Node::~Node() {
  for (int i = 0; i < 4; i++) {
    if (blk_bufs[i] != nullptr) {
      free(blk_bufs[i]);
    }
  }
}

ck::ref<mm::VMObject> ext2::FileNode::mmap(fs::File &f, size_t npages, int prot, int flags, off_t off) {
  EXT_DEBUG("mmap\n");
  // XXX: this is invalid, should be asserted before here :^)
  if (off & 0xFFF) return nullptr;

  // if (flags & MAP_PRIVATE) printk("ext2 map private\n");
  // if (flags & MAP_SHARED) printk("ext2 map shared\n");

  return ck::make_ref<ext2_vmobject>(f.ino, npages, off);
}




void ext2::DirectoryNode::ensure(void) {
  EXT_DEBUG("ensure\n");
  if (entries.size() == 0) {
    ext2::FileSystem *efs = static_cast<ext2::FileSystem *>(sb.get());
    ext2_traverse_dir(*this, [&](uint32_t ino, const char *name) {
      auto n = efs->get_inode(ino);
			ck::string newname = name;
      link(name, n);
      EXT_DEBUG(" - %d: %s\n", ino, name);
    });
  }
}


int ext2::DirectoryNode::touch(ck::string name, fs::Ownership &own) {
  ensure();
  UNIMPL();
  return -ENOTIMPL;
}

int ext2::DirectoryNode::mkdir(ck::string name, fs::Ownership &own) {
  ensure();
  UNIMPL();
  return -ENOTIMPL;
}

int ext2::DirectoryNode::unlink(ck::string name) {
  ensure();
  UNIMPL();
  return -ENOTIMPL;
}

ck::vec<fs::DirectoryEntry *> ext2::DirectoryNode::dirents(void) {
  EXT_DEBUG("dirents\n");
  ensure();
  ck::vec<fs::DirectoryEntry *> res;
  for (auto &[name, ent] : entries) {
    res.push(ent.get());
  }
  return res;
}

fs::DirectoryEntry *ext2::DirectoryNode::get_direntry(ck::string name) {
  EXT_DEBUG("get_direntry %s\n", name.get());
  ensure();
  auto f = entries.find(name);
  if (f == entries.end()) return nullptr;
  return f->value.get();
}

int ext2::DirectoryNode::link(ck::string name, ck::ref<fs::Node> node) {
  EXT_DEBUG("link %s\n", name.get());
  if (entries.contains(name)) return -EEXIST;
  auto ent = ck::make_box<fs::DirectoryEntry>(name, node);
  entries[name] = move(ent);
  return 0;
}




static int ext2_mknod(fs::Node &, const char *name, struct fs::Ownership &, int major, int minor) {
  UNIMPL();
  return -ENOTIMPL;
}


static ck::ref<fs::Node> ext2_inode(int type, u32 index, ext2::FileSystem &fs) {
  ck::ref<fs::Node> inode = nullptr;
  switch (type) {
    default:
      KWARN("ext2: cannot handle non T_DIR or T_FILE. Got %d\n", type);
    case T_FILE:
      inode = ck::make_ref<ext2::FileNode>(&fs);
      break;
    case T_DIR:
      inode = ck::make_ref<ext2::DirectoryNode>(&fs);
      break;
  }

  inode->set_inode(index);
  return inode;
}

ck::ref<fs::Node> ext2::FileSystem::create_inode(u32 index) {
  ck::ref<fs::Node> ino = nullptr;

  ext2::ext2_inode_info info;
  read_inode(info, index);

  int ino_type = T_INVA;

  auto type = ((info.type) & 0xF000) >> 12;

  if (type == 0x1) ino_type = T_FIFO;
  if (type == 0x2) ino_type = T_CHAR;
  if (type == 0x4) ino_type = T_DIR;
  if (type == 0x6) ino_type = T_BLK;
  if (type == 0x8) ino_type = T_FILE;
  if (type == 0xA) ino_type = T_SYML;
  if (type == 0xC) ino_type = T_SOCK;

  ino = ext2_inode(ino_type, index, *this);

  injest_info(*ino, info);
  ino->set_inode(index);

  return ino;
}
