#ifndef __ext2_H__
#define __ext2_H__

#include <dev/blk_dev.h>
#include <fs/filesystem.h>
#include <fs/inode.h>
#include <func.h>
#include <vec.h>

/*
 * Ext2 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */
#define EXT2_FT_UNKNOWN 0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR 2
#define EXT2_FT_CHRDEV 3
#define EXT2_FT_BLKDEV 4
#define EXT2_FT_FIFO 5
#define EXT2_FT_SOCK 6
#define EXT2_FT_SYMLINK 7

#define EXT2_FT_MAX 8

namespace fs {

class ext2;

/**
 * represents the actual structure on-disk of an inode.
 *
 * The fs will stamp the disk data here to populate it, and stamp it out to
 * write
 */
struct [[gnu::packed]] ext2_inode_info {
  uint16_t type;
  uint16_t uid;
  uint32_t size;
  uint32_t last_access;
  uint32_t create_time;
  uint32_t last_modif;
  uint32_t delete_time;
  uint16_t gid;
  uint16_t hardlinks;
  uint32_t disk_sectors;
  uint32_t flags;
  uint32_t ossv1;
  uint32_t dbp[12];
  uint32_t singly_block;
  uint32_t doubly_block;
  uint32_t triply_block;
  uint32_t gen_no;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t fragment_block;
  uint8_t ossv2[12];
};

class ext2_inode : public inode {
  friend class ext2;

 public:
  explicit ext2_inode(ext2 &fs, u32 index);
  virtual ~ext2_inode() override;

 protected:
  ext2_inode_info info;
};

class ext2 final : public filesystem {
 public:
  ext2(dev::blk_dev &);

  ~ext2(void);

  virtual bool init(void);
  virtual ref<inode> get_inode(u32 index);

  int read_file(u32 inode, u32 off, u32 len, u8 *buf);


  virtual ref<fs::inode> open(fs::path, u32 flags);

 private:
  bool read_block(u32 block, void *buf);
  bool write_block(u32 block, const void *buf);

  bool read_inode(ext2_inode *dst, u32 inode);
  bool read_inode(ext2_inode_info *dst, u32 inode);
  bool read_inode(ext2_inode_info &dst, u32 inode);

  bool write_inode(ext2_inode *dst, u32 inode);
  bool write_inode(ext2_inode_info *dst, u32 inode);
  bool write_inode(ext2_inode_info &dst, u32 inode);

  // must free the result of this.
  void *read_entire(ext2_inode_info &inode);

  vec<fs::directory_entry> read_dir(u32 inode);
  vec<fs::directory_entry> read_dir(ext2_inode_info &inode);
  void traverse_dir(u32 inode, func<bool(fs::directory_entry)> callback);
  void traverse_dir(ext2_inode_info &inode,
                    func<bool(fs::directory_entry)> callback);
  void traverse_blocks(vec<u32>, void *, func<bool(void *)> callback);

  // entrypoint to read a file
  int read_file(ext2_inode_info &inode, u32 off, u32 len, u8 *buf);

  vec<u32> blocks_for_inode(u32 inode);
  vec<u32> blocks_for_inode(ext2_inode_info &inode);

  // internal. Defined in src/fs/ext2.cpp
  struct superblock;
  dev::blk_dev &dev;

  superblock *sb = nullptr;

  // how many bytes a block is made up of
  u32 blocksize = 0;

  // how many blockgroups are in the filesystem
  u32 blockgroups = 0;

  // the location of the first block group descriptor
  u32 first_bgd = 0;

  // blocks are read here for access. This is so the filesystem can cut down on
  // allocations when doing general maintainence
  void *work_buf = nullptr;
  void *inode_buf = nullptr;
};
}  // namespace fs

#endif
