#include <asm.h>
#include <time.h>
#include <dev/disk.h>
#include <errno.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
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
}

bool fs::ext2::init(fs::blkdev *bdev) {
  TRACE;

  fs::blkdev::acquire(bdev);
  this->bdev = bdev;
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


  sb->last_mount = time::now_ms() / 1000;
  // solve for the filesystems block size
  block_size = 1024 << sb->blocksize_hint;

  if (block_size != 4096) {
    printk(KERN_WARN "ext2: blocksize is not 4096\n");
    return false;
  }

  // the number of block groups can be found by rounding up the
  // blocks/blocks_per_group
  blockgroups = ceil_div((int)sb->blocks, (int)sb->blocks_in_blockgroup);


  first_bgd = block_size == 1024 ? 2 : 1;


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
	auto bgd_bb = bref::get(*bdev, first_bgd);
  auto *bgd = (block_group_desc *)bgd_bb->data() + bg;

  // find the index and seek to the inode
  u32 index = (inode - 1) % sb->inodes_in_blockgroup;
  u32 block = (index * sb->s_inode_size) / block_size;


	auto inode_bb = bref::get(*bdev, bgd->inode_table + block);

  auto *_inode =
      (ext2_inode_info *)inode_bb->data() + (index % (block_size / sb->s_inode_size));

  memcpy(&dst, _inode, sizeof(ext2_inode_info));

  return true;
}

bool fs::ext2::write_inode(ext2_inode_info &src, u32 inode) {
  TRACE;
  u32 bg = (inode - 1) / sb->inodes_in_blockgroup;

	auto bgd_bb = bref::get(*bdev, first_bgd);
  auto *bgd = (block_group_desc *)bgd_bb->data() + bg;

  // find the index and seek to the inode
  u32 index = (inode - 1) % sb->inodes_in_blockgroup;
  u32 block = (index * sb->s_inode_size) / block_size;

	auto inode_bb = bref::get(*bdev, bgd->inode_table + block);

  auto *_inode =
      (ext2_inode_info *)inode_bb->data() + (index % (block_size / sb->s_inode_size));

  memcpy(_inode, &src, sizeof(ext2_inode_info));
	inode_bb->register_write();

  return true;
}



#define SETBIT(n) (1 << (((n)&0b111)))
#define BLOCKBYTE(bg_buffer, n) (bg_buffer[((n) >> 3)])
#define BLOCKBIT(bg_buffer, n) (BLOCKBYTE(bg_buffer, n) & SETBIT(n))

long fs::ext2::allocate_inode(void) {
  scoped_lock l(bglock);

  int bgs = blockgroups;
  int res = -1;

  // now that we have which BGF the inode is in, load that desc
	auto first_bgd_bb = bref::get(*bdev, first_bgd);

  // space for the bitmap (a little wasteful with memory, but fast)
  //    (allocates a full page)

  int nfree = 0;

  for (int i = 0; i < bgs; i++) {
    auto *bgd = (block_group_desc *)first_bgd_bb->data() + i;

    if (res == -1 && bgd->num_of_unalloc_inode > 0) {

			auto bitmap_bb = bref::get(*bdev, bgd->inode_bitmap);
      auto bitmap = (char *)bitmap_bb->data();

      int j = 0;
      for (; j < sb->inodes_in_blockgroup && BLOCKBIT(bitmap, j); j++) {
      }
      // evaluate to the actual inode number
      res = j + i * sb->inodes_in_blockgroup + 1;
      bitmap[j / 8] |= static_cast<u8>((1u << (j % 8)));
			bitmap_bb->register_write();
      sb->unallocatedinodes--;
      bgd->num_of_unalloc_inode--;
			first_bgd_bb->register_write();
			write_superblock();
			return res;
    }

    nfree += bgd->num_of_unalloc_inode;
  }

  return 0;
}



uint32_t fs::ext2::balloc(void) {
  scoped_lock l(bglock);

  unsigned int block_no = 0;

  auto first_bgd_bb = bref::get(*bdev, first_bgd);
  auto *bgd = (block_group_desc *)first_bgd_bb->data();

  auto blocks_in_group = sb->blocks_in_blockgroup;


  for (uint32_t bg_idx = 0; bg_idx < blockgroups; bg_idx++) {
    if (bgd[bg_idx].num_of_unalloc_block == 0) continue;

    auto bgblk = bref::get(*bdev, bgd[bg_idx].block_bitmap);
    auto bg_buffer = bgblk->data();

		// hexdump(bg_buffer, block_size, true);

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

        write_superblock();


				// clear out the new block we just allocated
				// TODO: allow this to happen without reading the block
        auto newblk = bref::get(*bdev, block_no);
        memset(newblk->data(), 0x00, block_size);

				// register everything we've changed :^)
        newblk->register_write();
        first_bgd_bb->register_write();
        bgblk->register_write();

        return block_no;
      }
      panic("Failed to find zero-bit");
    }
  }


  printk(KERN_WARN "No Space left on disk\n");
  return 0;
}




void fs::ext2::bfree(uint32_t block) {
  scoped_lock l(m_lock);

  //
}

bool fs::ext2::read_block(u32 block, void *buf) {
  bread(*bdev, (void *)buf, block_size, block * block_size);
  return true;
}

bool fs::ext2::write_block(u32 block, const void *buf) {
  bwrite(*bdev, (void *)buf, block_size, block * block_size);
  return true;
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
