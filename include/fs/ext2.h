#ifndef __ext2_H__
#define __ext2_H__

#include <dev/blk_cache.h>
#include <dev/blk_dev.h>
#include <fs/filesystem.h>
#include <fs/vnode.h>
#include <func.h>
#include <lock.h>
#include <map.h>
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

class ext2_inode : public vnode {
  friend class ext2;

 public:
  explicit ext2_inode(ext2 &fs, u32 index);
  virtual ~ext2_inode();

  virtual file_metadata metadata(void);

  virtual int add_dir_entry(ref<vnode> node, const string &name, u32 mode);
  virtual ssize_t read(filedesc &, void *, size_t);
  virtual ssize_t write(filedesc &, void *, size_t);
  virtual int truncate(size_t newlen);

  // return the block size of the fs
  int block_size();

  inline virtual fs::filesystem &fs() { return m_fs; }

 protected:
  int cached_path[4] = {0, 0, 0, 0};
  int *blk_bufs[4] = {NULL, NULL, NULL, NULL};

  ssize_t do_rw(fs::filedesc &, size_t, void *, bool is_write);
  off_t block_for_byte(off_t b);

  // return the ith block's index, returning 0 on failure.
  // if set_to is passed and is non-zero, the block at that
  // index will be written as set_to
  int block_from_index(int i_block, int set_to = 0);
  ext2_inode_info info;
  virtual bool walk_dir_impl(func<bool(const string &, u32)> cb);

  fs::filesystem &m_fs;
};

/**
 * An ext2 implementation of the filesystem class
 *
 *
 * Allows general read/write to the filesystem located on a block device
 * provided in the constructor.
 *
 * It implements the standard Second Extended Filesystem
 */
class ext2 final : public filesystem {
 public:
  ext2(ref<dev::device>);

  ~ext2(void);

  virtual bool init(void);

  virtual vnoderef get_root_inode(void);
  virtual vnoderef get_inode(u32 index);

  int read_file(u32 inode, u32 off, u32 len, u8 *buf);

  inline virtual u64 block_size(void) { return blocksize; }

  friend class ext2_inode;
  bool read_block(u32 block, void *buf);
  bool write_block(u32 block, const void *buf);

  bool read_inode(ext2_inode *dst, u32 inode);
  bool read_inode(ext2_inode_info *dst, u32 inode);
  bool read_inode(ext2_inode_info &dst, u32 inode);

  bool write_inode(ext2_inode *dst, u32 inode);
  bool write_inode(ext2_inode_info *dst, u32 inode);
  bool write_inode(ext2_inode_info &dst, u32 inode);

  // must free the result of this.
  // void *read_entire(ext2_inode_info &inode);

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

  // update the disk copy of the superblock
  int write_superblock(void);

  struct [[gnu::packed]] superblock {
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
  ref<dev::blk_dev> dev;

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

  vnoderef m_root_inode{};

  dev::blk_cache disk_cache;

  map<u32, fs::vnoderef> vnode_cache;

  mutex_lock m_lock;
};
}  // namespace fs

#endif
