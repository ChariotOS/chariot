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
  uint16_t num_of_dirs_inode;
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

}

bool fs::ext2::init(fs::blkdev *bdev) {
  TRACE;

  fs::blkdev::acquire(bdev);
  disk = fs::bdev_to_file(bdev);

  sector_size = bdev->block_size;

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


  scoped_lock l1(inode_buf_lock);
  scoped_lock l2(work_buf_lock);

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

  scoped_lock l1(inode_buf_lock);
  scoped_lock l2(work_buf_lock);


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



#define SETBIT(n) (1 << (((n)&0b111)))
#define BLOCKBYTE(bg_buffer, n) (bg_buffer[((n) >> 3)])
#define BLOCKBIT(bg_buffer, n) (BLOCKBYTE(bg_buffer, n) & SETBIT(n))

long fs::ext2::allocate_inode(void) {
  scoped_lock l(bglock);

  int bgs = blockgroups;
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


  if (flush_changes) {
    write_block(first_bgd, work_buf);
    write_superblock();
  }

  phys::kfree(vbitmap, 1);

  return res;
}



u32 fs::ext2::balloc(void) {
  scoped_lock l(bglock);

  unsigned int block_no = 0;
  auto *bg_buffer = (uint8_t *)kmalloc(block_size);
  auto *first_bgd_block = (uint8_t *)kmalloc(block_size);

  auto blocks_in_group = sb->blocks_in_blockgroup;
  read_block(first_bgd, first_bgd_block);


  auto *bgd = (block_group_desc *)first_bgd_block;


  for (uint32_t bg_idx = 0; bg_idx < blockgroups; bg_idx++) {
    if (bgd[bg_idx].num_of_unalloc_block == 0) continue;

    read_block(bgd[bg_idx].block_bitmap, bg_buffer);

    auto words = reinterpret_cast<uint32_t *>(bg_buffer);

    for (int i = 0; i < (blocks_in_group + 31) / 32; i++) {
      if (words[i] == 0xFFFFFFFF) continue;

      for (int bit = 0; bit < 32; bit++) {
        if (words[i] & (1 << bit)) continue;

        block_no = (bg_idx * blocks_in_group) + (i * 32) + bit;
				block_no += sb->first_data_block;
        bgd[bg_idx].num_of_unalloc_block--;
        sb->unallocatedblocks--;
        assert(block_no);
        words[i] |= static_cast<uint32_t>(1) << bit;

        // flush the bitmap we just changed
        write_block(bgd[bg_idx].block_bitmap, words);
        write_block(first_bgd, bgd);

        write_superblock();

				/*
				printk("%d:\n", block_no-1);
        read_block(block_no-1, bg_buffer);
				hexdump(bg_buffer, block_size, true);
				*/

        memset(bg_buffer, 0x00, block_size);
        write_block(block_no, bg_buffer);

        kfree(bg_buffer);
        kfree(first_bgd_block);

        return block_no;
      }
      panic("Failed to find zero-bit");
    }
  }

  printk(KERN_WARN "No Space left on disk\n");
  kfree(bg_buffer);
  kfree(first_bgd_block);
  return 0;
}




void fs::ext2::bfree(u32 block) {
  scoped_lock l(m_lock);

  //
}

bool fs::ext2::read_block(u32 block, void *buf) {
  disk->seek(block * block_size, SEEK_SET);
  return disk->read(buf, block_size);
}

bool fs::ext2::write_block(u32 block, const void *buf) {
  disk->seek(block * block_size, SEEK_SET);
  return disk->write((void *)buf, block_size);
}

struct fs::inode *fs::ext2::get_root(void) {
  return root;
}

struct fs::inode *fs::ext2::get_inode(u32 index) {
  TRACE;
  scoped_lock lck(m_lock);
  if (inodes[index] == NULL) {
    inodes[index] = fs::ext2::create_inode(this, index);
  }
  return inodes[index];
}

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
