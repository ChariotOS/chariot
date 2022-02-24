#ifndef __ext2_H__
#define __ext2_H__

#include <dev/disk.h>
#include <fs.h>
#include <ck/func.h>
#include <lock.h>
#include <ck/map.h>
#include <ck/vec.h>
#include "types.h"


// #define ENABLE_EXT2_DEBUG

#ifdef ENABLE_EXT2_DEBUG
#define EXT_DEBUG(...) PFXLOG(GRN "EXT2", __VA_ARGS__)
#else
#define EXT_DEBUG(fmt, args...)
#endif

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


template <typename T, typename U>
inline constexpr T ceil_div(T a, U b) {
  static_assert(sizeof(T) == sizeof(U));
  T result = a / b;
  if ((a % b) != 0) ++result;
  return result;
}


namespace ext2 {


  class FileSystem;

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
    union {
      struct {
        uint32_t dbp[12];
        uint32_t singly_block;
        uint32_t doubly_block;
        uint32_t triply_block;
      };
      uint32_t block_pointers[12 + 3];
    };
    uint32_t gen_no;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t fragment_block;
    uint8_t ossv2[12];
  };



  class Node : public fs::Node {
   public:
    using fs::Node::Node;
    ext2::ext2_inode_info info;
    spinlock path_cache_lock;
    int cached_path[4] = {0, 0, 0, 0};
    int *blk_bufs[4] = {NULL, NULL, NULL, NULL};
    virtual ~Node(void);
  };

  class FileNode : public ext2::Node {
   public:
    using ext2::Node::Node;
		virtual ~FileNode() {}
    virtual bool is_file(void) final { return true; }

    virtual int seek(fs::File &, off_t old_off, off_t new_off);
    virtual ssize_t read(fs::File &, char *dst, size_t count);
    virtual ssize_t write(fs::File &, const char *, size_t);
    virtual int resize(fs::File &, size_t);
    virtual ssize_t size(void);

    virtual ck::ref<mm::VMObject> mmap(fs::File &, size_t npages, int prot, int flags, off_t off);
  };


  class DirectoryNode : public ext2::Node {
   public:
    using ext2::Node::Node;
		virtual ~DirectoryNode() {}
    virtual bool is_dir(void) final { return true; }

    ck::map<ck::string, ck::box<fs::DirectoryEntry>> entries;

		// Entries are loaded lazily. this method does that.
		void ensure(void);

    virtual int touch(ck::string name, fs::Ownership &);
    virtual int mkdir(ck::string name, fs::Ownership &);
    virtual int unlink(ck::string name);
    virtual ck::vec<fs::DirectoryEntry *> dirents(void);
    virtual fs::DirectoryEntry *get_direntry(ck::string name);
    virtual int link(ck::string name, ck::ref<fs::Node> node);
  };

  class FileSystem final : public fs::FileSystem {
   public:
    struct [[gnu::packed]] superblock {
      uint32_t inodes;
      uint32_t blocks;
      uint32_t reserved_for_root;
      uint32_t unallocatedblocks;
      uint32_t unallocatedinodes;
      uint32_t first_data_block;
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

      uint32_t s_first_ino;              /* First non-reserved inode */
      uint16_t s_inode_size;             /* size of inode structure */
      uint16_t s_block_group_nr;         /* block group # of this superblock */
      uint32_t s_feature_compat;         /* compatible feature set */
      uint32_t s_feature_incompat;       /* incompatible feature set */
      uint32_t s_feature_ro_compat;      /* readonly-compatible feature set */
      uint8_t s_uuid[16];                /* 128-bit uuid for volume */
      char s_volume_name[16];            /* volume name */
      char s_last_mounted[64];           /* directory where last mounted */
      uint32_t s_algorithm_usage_bitmap; /* For compression */
      /*
       * Performance hints.  Directory preallocation should only
       * happen if the EXT2_FEATURE_COMPAT_DIR_PREALLOC flag is on.
       */
      uint8_t s_prealloc_blocks;      /* Nr of blocks to try to preallocate*/
      uint8_t s_prealloc_dir_blocks;  /* Nr to preallocate for dirs */
      uint16_t s_reserved_gdt_blocks; /* Per group table for online growth */
      /*
       * Journaling support valid if EXT2_FEATURE_COMPAT_HAS_JOURNAL set.
       */
      uint8_t s_journal_uuid[16]; /* uuid of journal superblock */
      uint32_t s_journal_inum;    /* inode number of journal file */
      uint32_t s_journal_dev;     /* device number of journal file */
      uint32_t s_last_orphan;     /* start of list of inodes to delete */
      uint32_t s_hash_seed[4];    /* HTREE hash seed */
      uint8_t s_def_hash_version; /* Default hash version to use */
      uint8_t s_jnl_backup_type;  /* Default type of journal backup */
      uint16_t s_desc_size;       /* Group desc. size: INCOMPAT_64BIT */
      uint32_t s_default_mount_opts;
      uint32_t s_first_meta_bg;      /* First metablock group */
      uint32_t s_mkfs_time;          /* When the filesystem was created */
      uint32_t s_jnl_blocks[17];     /* Backup of the journal inode */
      uint32_t s_blocks_count_hi;    /* Blocks count high 32bits */
      uint32_t s_r_blocks_count_hi;  /* Reserved blocks count high 32 bits*/
      uint32_t s_free_blocks_hi;     /* Free blocks count */
      uint16_t s_min_extra_isize;    /* All inodes have at least # bytes */
      uint16_t s_want_extra_isize;   /* New inodes should reserve # bytes */
      uint32_t s_flags;              /* Miscellaneous flags */
      uint16_t s_raid_stride;        /* RAID stride */
      uint16_t s_mmp_interval;       /* # seconds to wait in MMP checking */
      uint64_t s_mmp_block;          /* Block for multi-mount protection */
      uint32_t s_raid_stripe_width;  /* blocks on all data disks (N*stride)*/
      uint8_t s_log_groups_per_flex; /* FLEX_BG group size */
      uint8_t s_reserved_char_pad;
      uint16_t s_reserved_pad;  /* Padding to next 32bits */
      uint32_t s_reserved[162]; /* Padding to the end of the block */
    };



    FileSystem(void);
    ~FileSystem(void);

    bool probe(ck::ref<fs::BlockDeviceNode>);

    // implemented in ext2/inode.cpp
    ck::ref<fs::Node> create_inode(u32 index);
    ck::ref<fs::Node> get_root(void);
    ck::ref<fs::Node> get_inode(u32 index);

    friend class ext2_inode;
    bool read_block(u32 block, void *buf);
    bool write_block(u32 block, const void *buf);

    bool read_inode(ext2_inode_info &dst, u32 inode);
    bool write_inode(ext2_inode_info &dst, u32 inode);

    inline struct block::Buffer *bget(uint32_t block) { return ::bget(*bdev, block); }

    uint32_t balloc(void);
    void bfree(uint32_t);

    // update the disk copy of the superblock
    int write_superblock(void);

    long allocate_inode(void);

    superblock *sb = nullptr;


    // lock the block groups
    spinlock bglock;

    // how many blockgroups are in the filesystem
    uint32_t blockgroups = 0;

    // the location of the first block group descriptor
    uint32_t first_bgd = 0;

    // blocks are read here for access. This is so the filesystem can cut down
    // on allocations when doing general maintainence

    // the size of a single disk sector
    long sector_size;

    ck::map<u32, ck::ref<fs::Node>> inodes;

    // how many entries in the disk cache
    int cache_size;
    int cache_time = 0;
    spinlock cache_lock;

    ck::box<fs::File> disk;
    ck::ref<fs::BlockDeviceNode> bdev;

    spinlock m_lock;
  };
}  // namespace ext2

#endif
