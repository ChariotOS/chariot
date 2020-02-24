#include <errno.h>
#include <fs.h>
#include <fs/ext2.h>

#define EXT2_ADDR_PER_BLOCK(node) (node->block_size / sizeof(u32))

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

fs::ext2_inode::ext2_inode(int type, fs::ext2 &fs, u32 index)
    : fs::inode(type), fs(fs) {
  this->ino = index;
}

fs::ext2_inode::~ext2_inode() {
  for (int i = 0; i < 4; i++) {
    if (blk_bufs[i] != nullptr) {
      kfree(blk_bufs[i]);
    }
  }
}

fs::ext2_inode *fs::ext2_inode::create(ext2 &fs, u32 index) {
  fs::ext2_inode *ino = NULL;

  struct ext2_inode_info info;

  fs.read_inode(info, index);

  int ino_type = T_INVA;

  auto type = ((info.type) & 0xF000) >> 12;

  if (type == 0x1) ino_type = T_FIFO;
  if (type == 0x2) ino_type = T_CHAR;
  if (type == 0x4) ino_type = T_DIR;
  if (type == 0x6) ino_type = T_BLK;
  if (type == 0x8) ino_type = T_FILE;
  if (type == 0xA) ino_type = T_SYML;
  if (type == 0xC) ino_type = T_SOCK;

  ino = new ext2_inode(ino_type, fs, index);
  ino->injest_info(info);

  return ino;
}

ssize_t fs::ext2_inode::do_read(fs::filedesc &d, void *dst, size_t nbytes) {
  return do_rw(d, nbytes, dst, false);
}

ssize_t fs::ext2_inode::do_write(fs::filedesc &d, void *dst, size_t nbytes) {
  return do_rw(d, nbytes, dst, true);
}


int fs::ext2_inode::touch(string name, int mode, fs::inode *&dst) {
  if (type != T_DIR) return -ENOTDIR;


  printk("ext2 touch\n");
  return -EINVAL;
}

/**
 * do_rw - actually execute the read/write of bytes on an inode
 *
 * @off: the byte offset into the file
 * @nbytes: how many bytes we want to read/write
 * @buf: the buffer that we wish to read into or write out of
 * @is_write: the direction of the action.
 */
ssize_t fs::ext2_inode::do_rw(fs::filedesc &d, size_t nbytes, void *buf,
                              bool is_write) {
  if (is_write) {
    // TODO: ensure blocks are avail
  }

  ssize_t offset = d.offset();
  if (offset > size) return 0;

  // how many bytes have been read
  ssize_t nread = 0;

  // the size of a single block
  auto bsize = fs.blocksize;

  off_t first_blk_ind = offset / bsize;
  off_t last_blk_ind = (offset + nbytes) / bsize;
  off_t offset_into_first_block = offset % bsize;

  // TODO: lock the FS. We now want to own the efs->work_buf
  auto *given_buf = (u8 *)buf;

  int remaining_count = min((off_t)nbytes, (off_t)size - offset);

  for (int bi = first_blk_ind; remaining_count && bi <= last_blk_ind; bi++) {
    u32 blk = block_from_index(bi);
    if (blk == 0) {
      printk("ext2fs: read_bytes: failed at lbi %u\n", bi);
      return -EIO;
    }

    auto *buf = (u8 *)fs.work_buf;
    fs.read_block(blk, buf);

    int offset_into_block = (bi == first_blk_ind) ? offset_into_first_block : 0;
    int num_bytes_to_copy = min(bsize - offset_into_block, remaining_count);

    if (is_write) {
      memcpy(buf + offset_into_block, given_buf, num_bytes_to_copy);
      fs.write_block(blk, buf);
    } else {
      memcpy(given_buf, buf + offset_into_block, num_bytes_to_copy);
    }
    remaining_count -= num_bytes_to_copy;
    nread += num_bytes_to_copy;
    given_buf += num_bytes_to_copy;
  }
  d.seek(nread, SEEK_CUR);
  return nread;
}

int fs::ext2_inode::block_from_index(int i_block, int set_to) {
  auto bsize = fs.blocksize;
  // start the inodeS
  auto table = (int *)block_pointers;
  int path[4];
  int n = block_to_path(this, i_block, path);

  for (int i = 0; i < n - 1; i++) {
    int off = path[i];
    if (blk_bufs[i] == NULL || cached_path[i] != off) {
      if (blk_bufs[i] == NULL) blk_bufs[i] = (int *)kmalloc(bsize);
      if (!fs.read_block(table[off], blk_bufs[i])) {
        return 0;
      }
      cached_path[i] = off;
    }
    table = blk_bufs[i];
  }

  return table[path[n - 1]];
}

int fs::ext2_inode::injest_info(fs::ext2_inode_info &info) {
  size = info.size;
  mode = info.type;
  uid = info.uid;
  gid = info.gid;
  link_count = info.hardlinks;
  atime = info.last_access;
  ctime = info.create_time;
  mtime = info.last_modif;
  dtime = info.delete_time;
  block_size = fs.blocksize;

  auto table = (u32 *)info.dbp;
  // copy the block pointers
  for (int i = 0; i < 15; i++) {
    block_pointers[i] = table[i];
  }

  if (type == T_DIR) {
    // TODO: clear out the dirents
    fs.traverse_dir(this->ino, [this](u32 ino, const char *name) -> bool {
      // register the resident entries
      this->register_direntry(name, ENT_RES);
      return true;
    });
  } else if (type == T_CHAR || type == T_BLK) {
    // parse the major/minor
    unsigned dev = info.dbp[0];
    if (!dev) dev = info.dbp[1];
    major = (dev & 0xfff00) >> 8;
    minor = (dev & 0xff) | ((dev >> 12) & 0xfff00);
  }
  return 0;
}

int fs::ext2_inode::commit_info(void) {
  fs::ext2_inode_info info;

  fs.read_inode(info, ino);

  info.size = size;
  info.uid = uid;
  info.gid = gid;
  info.hardlinks = link_count;
  info.last_access = atime;
  info.create_time = ctime;
  mtime = info.last_modif = dev::RTC::now();

  info.type = (info.type & ~0xFFF) | (mode & 0xFFF);

  // ??
  info.delete_time = dtime;

  // copy the block pointers
  auto table = (u32 *)info.dbp;
  for (int i = 0; i < 15; i++) {
    table[i] = block_pointers[i];
  }

  fs.write_inode(info, ino);

  return 0;
}

struct fs::inode *fs::ext2_inode::resolve_direntry(const char *needle) {
  bool found = false;
  u32 ent_inode_num;

  fs.traverse_dir(this->ino, [&](u32 ino, const char *name) -> bool {
    if (!strcmp(needle, name)) {
      ent_inode_num = ino;
      found = true;
      return false;
    }
    return true;
  });

  if (found) return ext2_inode::create(fs, ent_inode_num);
  return NULL;
}

int fs::ext2_inode::rm(string &name) {
  // TODO:
  return -1;
}
