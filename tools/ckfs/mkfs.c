#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


#define CKFS_FT_REG 1
#define CKFS_FT_DIR 2


struct ckfs_superblock {
  uint32_t total_inodes;
  uint32_t avail_inodes;

  uint32_t total_blocks;
  uint32_t avail_blocks;

  /* The start of the inode bitmap */
  uint64_t inode_bitmap;
  /* The start of the block bitmap */
  uint64_t block_bitmap;
  /* Offset of the first data block into the device */
  uint64_t first_data_block;
};

struct ckfs_inode {
  /* first word */
  unsigned type : 7;
  unsigned prot : 9; /* Typical unix protection */
  uint16_t links;
  /* Second word */
  uint16_t uid, gid; /* User and group id */

  uint64_t size;
  uint32_t time_created;
  uint32_t time_modified;
  uint32_t resv[1]; /* OS Specific */
  /* A pointer to the start of the block that contains the block list */
  uint32_t blocklist;
} __attribute__((packed));

struct ext2_inode {
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
} __attribute__((packed));

int main(int argc, char **argv) {
  printf("\n\n\n");
  printf("================================\n");

  printf("[ckfs] inode size: %zu. Per page: %f\n", sizeof(struct ckfs_inode),
         4096 / (float)sizeof(struct ckfs_inode));
  printf("[ext2] inode size: %zu. Per page: %f\n", sizeof(struct ext2_inode),
         4096 / (float)sizeof(struct ext2_inode));

  int fd = open(argv[1], O_RDWR);

  uint64_t sz;
  /* This magic thing... */
  ioctl(fd, BLKGETSIZE64, &sz);

  void *disk = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  printf("%p\n", disk);
  close(fd);

  printf("================================\n");
  printf("\n\n\n");

  return 0;
}
