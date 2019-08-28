#include <fs/file.h>
#include <fs/filesystem.h>

// auto incrementing id for every filesystem. Used to uniquely identify a
// filesystem quickly. Though this does mean there can only ever be 2^32
// mounted filesystems ever... oh well
static u32 next_fsid = 0;

fs::filesystem::filesystem(void) {
  // TODO: take a lock
  m_fsid = next_fsid++;
}

fs::filesystem::~filesystem(void) {}

ref<fs::vnode> fs::filesystem::open(string path, u32 flags) {
  fs::path p = path;
  return open(p, flags);
}
