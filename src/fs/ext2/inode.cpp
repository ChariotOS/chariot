#include <errno.h>
#include <fs/ext2.h>

#define EXT2_ADDR_PER_BLOCK(node) (node->block_size() / sizeof(u32))

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

struct block_reader {
  fs::ext2_inode &node;

  int last_path[4];

 public:
  block_reader(fs::ext2_inode &node) : node(node) {}
};

#define EXT2_NDIR_BLOCKS 12
#define EXT2_IND_BLOCK EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS (EXT2_TIND_BLOCK + 1)
/**
 * block_to_path - given a block number, solve the indirect path to it
 *
 * @node: the inode you are looking into. This argument exists to get metadata,
 *        and no IO occurs
 * @block: the block index to find the path of
 * @offsets: the indirect path array to fill in
 */
static int block_to_path(fs::ext2_inode *node, int i_block, int offsets[4],
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
    printk("[EXT2 WARN] %s: block is too big!\n", __func__);
  }

  if (boundary) *boundary = final - 1 - (i_block & (ptrs - 1));
  return n;
}

fs::ext2_inode::ext2_inode(fs::ext2 &fs, u32 index) : vnode(fs, index) {}

fs::ext2_inode::~ext2_inode() {
  for (int i = 0; i < 4; i++) {
    if (blk_bufs[i] != nullptr) {
      kfree(blk_bufs[i]);
    }
  }
}

int fs::ext2_inode::block_size() { return fs().block_size(); }

bool fs::ext2_inode::walk_dir_impl(func<bool(const string &, u32)> cb) {
  auto *efs = static_cast<ext2 *>(&fs());

  efs->traverse_dir(this->info, [&](fs::directory_entry de) -> bool {
    return cb(de.name, de.inode);
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
  return do_rw(off, nbytes, dst, false);
}

ssize_t fs::ext2_inode::write(off_t off, size_t nbytes, void *dst) {
  return do_rw(off, nbytes, dst, true);
}

/**
 * do_rw - actually execute the read/write of bytes on an inode
 *
 * @off: the byte offset into the file
 * @nbytes: how many bytes we want to read/write
 * @buf: the buffer that we wish to read into or write out of
 * @is_write: the direction of the action.
 */
ssize_t fs::ext2_inode::do_rw(off_t off, size_t nbytes, void *buf,
                              bool is_write) {
  // you can't call this function on directories
  if (is_dir()) return -EISDIR;

  if (is_write) {
    // TODO: ensure blocks are avail
  }

  auto &efs = static_cast<ext2 &>(fs());

  ssize_t offset = off;
  if (offset > info.size) return 0;

  // how many bytes have been read
  ssize_t nread = 0;

  // the size of a single block
  auto bsize = efs.block_size();

  off_t first_blk_ind = offset / bsize;
  off_t last_blk_ind = (offset + nbytes) / bsize;
  off_t offset_into_first_block = offset % bsize;

  // TODO: lock the FS. We now want to own the efs->work_buf
  auto *out = (u8 *)buf;

  int remaining_count = min((off_t)nbytes, (off_t)size() - off);

  // read the block into a buffer
  auto get_ith_block = [&](int i_block, bool &succ) -> u32 {
    // the current block table. Starting out as the block pointers from the
    // inode
    u32 *table = (u32 *)info.dbp;
    int path[4];
    int n = block_to_path(this, i_block, path);

    for (int i = 1; i < n; i++) {
      int off = path[i];
      if (true || blk_bufs[i] == NULL || cached_path[i] != off) {
        if (blk_bufs[i] == NULL) blk_bufs[i] = (u32 *)kmalloc(bsize);

        bool succ = efs.read_block(table[off], blk_bufs[i]);
        if (!succ) {
          succ = false;
          return 0;
        }
        cached_path[i] = off;
      }

      table = blk_bufs[i];
    }

    succ = true;

    return table[path[n - 1]];
  };

  for (int bi = first_blk_ind; remaining_count && bi <= last_blk_ind; bi++) {
    auto *buf = (u8 *)efs.work_buf;
    bool valid;
    u32 blk = get_ith_block(bi, valid);
    if (!valid) {
      printk("ext2fs: read_bytes: failed at lbi %u\n", bi);
      return -EIO;
    }

    efs.read_block(blk, buf);

    int offset_into_block = (bi == first_blk_ind) ? offset_into_first_block : 0;
    int num_bytes_to_copy = min(bsize - offset_into_block, remaining_count);
    memcpy(out, buf + offset_into_block, num_bytes_to_copy);
    remaining_count -= num_bytes_to_copy;
    nread += num_bytes_to_copy;
    out += num_bytes_to_copy;
  }
  return nread;
}
