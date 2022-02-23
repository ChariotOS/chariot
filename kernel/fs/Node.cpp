#include <asm.h>
#include <dev/driver.h>
#include <mm.h>
#include <errno.h>
#include <fs.h>
#include <mem.h>
#include <module.h>
#include <printk.h>
#include <net/sock.h>


fs::Node::Node(ck::ref<fs::FileSystem> sb) : sb(sb) {}
fs::Node::~Node() {}

// Default implementations for fs::Node
int fs::Node::seek(fs::File &, off_t old_off, off_t new_off) { return -ENOTIMPL; }
ssize_t fs::Node::read(fs::File &, char *dst, size_t count) { return -ENOTIMPL; }
ssize_t fs::Node::write(fs::File &, const char *, size_t) { return -ENOTIMPL; }
int fs::Node::ioctl(fs::File &, unsigned int, off_t) { return -ENOTIMPL; }
int fs::Node::open(fs::File &) { return 0; }
void fs::Node::close(fs::File &) {}
ck::ref<mm::VMObject> fs::Node::mmap(fs::File &, size_t npages, int prot, int flags, off_t off) { return nullptr; }
int fs::Node::resize(fs::File &, size_t) { return -ENOTIMPL; }
int fs::Node::stat(fs::File &, struct stat *) { return -ENOTIMPL; }
int fs::Node::poll(fs::File &, int events, poll_table &pt) { return 0; }
ssize_t fs::Node::size(void) {
  KWARN("fs::Node::size() called. Please implement it on the subclass\n");
  return 0;
}
