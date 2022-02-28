#include <asm.h>
#include <dev/disk.h>
#include <errno.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
#include <mem.h>
#include <module.h>
#include <phys.h>
#include <ck/string.h>
#include <time.h>
#include <util.h>
#include "fs.h"


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

ext2::FileSystem::FileSystem(void) { TRACE; }

ext2::FileSystem::~FileSystem(void) {
  TRACE;
  if (sb != nullptr) delete sb;
}

bool ext2::FileSystem::probe(ck::ref<dev::BlockDevice> bdev) {
  TRACE;

  this->bdev = bdev;

  sector_size = bdev->block_size();


  sb = new superblock();
  memset(sb, 0xFA, sizeof(*sb));

  disk = ck::make_box<fs::File>(bdev, 0);
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

  if (!write_superblock()) {
    printk("failed to write superblock\n");
    return false;
  }

  return true;
}

int ext2::FileSystem::write_superblock(void) {
  // TODO: lock
  disk->seek(1024, SEEK_SET);
  return disk->write(sb, 1024);
}

bool ext2::FileSystem::read_inode(ext2_inode_info &dst, u32 inode) {
  TRACE;
  u32 bg = (inode - 1) / sb->inodes_in_blockgroup;
  auto bgd_bb = bref::get(*bdev, first_bgd);
  auto *bgd = (block_group_desc *)bgd_bb->data() + bg;

  // find the index and seek to the inode
  u32 index = (inode - 1) % sb->inodes_in_blockgroup;
  u32 block = (index * sb->s_inode_size) / block_size;


  auto inode_bb = bref::get(*bdev, bgd->inode_table + block);

  auto *_inode = (ext2_inode_info *)inode_bb->data() + (index % (block_size / sb->s_inode_size));

  memcpy(&dst, _inode, sizeof(ext2_inode_info));

  return true;
}

bool ext2::FileSystem::write_inode(ext2_inode_info &src, u32 inode) {
  TRACE;
  u32 bg = (inode - 1) / sb->inodes_in_blockgroup;

  auto bgd_bb = bref::get(*bdev, first_bgd);
  auto *bgd = (block_group_desc *)bgd_bb->data() + bg;

  // find the index and seek to the inode
  u32 index = (inode - 1) % sb->inodes_in_blockgroup;
  u32 block = (index * sb->s_inode_size) / block_size;

  auto inode_bb = bref::get(*bdev, bgd->inode_table + block);

  auto *_inode = (ext2_inode_info *)inode_bb->data() + (index % (block_size / sb->s_inode_size));

  memcpy(_inode, &src, sizeof(ext2_inode_info));
  inode_bb->register_write();

  return true;
}



#define SETBIT(n) (1 << (((n)&0b111)))
#define BLOCKBYTE(bg_buffer, n) (bg_buffer[((n) >> 3)])
#define BLOCKBIT(bg_buffer, n) (BLOCKBYTE(bg_buffer, n) & SETBIT(n))

long ext2::FileSystem::allocate_inode(void) {
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



uint32_t ext2::FileSystem::balloc(void) {
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




void ext2::FileSystem::bfree(uint32_t block) {
  scoped_lock l(m_lock);

  //
}

bool ext2::FileSystem::read_block(u32 block, void *buf) {
  bread(*bdev, (void *)buf, block_size, block * block_size);
  return true;
}

bool ext2::FileSystem::write_block(u32 block, const void *buf) {
  bwrite(*bdev, (void *)buf, block_size, block * block_size);
  return true;
}

ck::ref<fs::Node> ext2::FileSystem::get_root(void) { return root; }

ck::ref<fs::Node> ext2::FileSystem::get_inode(u32 index) {
  TRACE;
  scoped_lock lck(m_lock);
  if (inodes[index] == nullptr) {
    inodes[index] = create_inode(index);
  }
  return inodes[index];
}


static ck::ref<fs::FileSystem> ext2_mount(ck::string args, int flags, ck::string device) {
  // open the device
  auto file = vfs::open(device);
  if (!file) {
    KWARN("EXT2: Cannot mount %s. File not found\n", device.get());
    return nullptr;
  }

  // if the device file is not a block file, we cannot mount it
  if (!file->is_blockdev()) {
    KWARN("EXT2: Cannot mount %s. Is not a block device\n", device.get());
    return nullptr;
  }

  ck::ref<dev::BlockDevice> bdev = file;
  auto filesystem = ck::make_ref<ext2::FileSystem>();
  if (!filesystem->probe(bdev)) {
    return nullptr;
  }

  return filesystem;
}

static void ext2_init(void) { vfs::register_filesystem("ext2", ext2_mount); }

module_init("fs::ext2", ext2_init);
