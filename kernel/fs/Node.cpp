#include <asm.h>
#include <dev/driver.h>
#include <mm.h>
#include <errno.h>
#include <fs.h>
#include <mem.h>
#include <module.h>
#include <printk.h>
#include <net/sock.h>


fs::Node::Node(ck::ref<fs::FileSystem> sb) : sb(sb) {
  if (sb) {
    // typically sane.
    set_block_size(sb->block_size);
  }
}
fs::Node::~Node() {}

// Default implementations for fs::Node
int fs::Node::seek_check(fs::File &, off_t old_off, off_t new_off) { return 0; }
ssize_t fs::Node::read(fs::File &, char *dst, size_t count) { return -ENOTIMPL; }
ssize_t fs::Node::write(fs::File &, const char *, size_t) { return -ENOTIMPL; }
int fs::Node::ioctl(fs::File &, unsigned int, off_t) { return -ENOTIMPL; }
int fs::Node::open(fs::File &) { return 0; }
void fs::Node::close(fs::File &) {}
ck::ref<mm::VMObject> fs::Node::mmap(fs::File &, size_t npages, int prot, int flags, off_t off) { return nullptr; }
int fs::Node::resize(fs::File &, size_t) { return -ENOTIMPL; }
int fs::Node::poll(fs::File &, int events, poll_table &pt) { return 0; }


int fs::Node::stat(struct stat *stat) {
  memset(stat, 0, sizeof(*stat));

  stat->st_ino = inode();
  stat->st_nlink = nlink();
  stat->st_size = size();
  stat->st_mode = mode() & 0777;

  if (is_dir()) stat->st_mode |= S_IFDIR;
  if (is_chardev()) stat->st_mode |= S_IFCHR;
  if (is_file()) stat->st_mode |= S_IFREG;
  if (is_blockdev()) stat->st_mode |= S_IFBLK;
  if (is_sock()) stat->st_mode |= S_IFSOCK;

  stat->st_uid = uid();
  stat->st_gid = gid();

  stat->st_blksize = block_size();
  stat->st_blocks = block_count();

  stat->st_atim = accessed_time();
  stat->st_mtim = modified_time();
  stat->st_ctim = create_time();

  return 0;
}

ck::ref<fs::Node> fs::Node::lookup(ck::string name) {
  auto ent = get_direntry(name);
  if (ent == nullptr) return nullptr;
  return ent->get();
}
