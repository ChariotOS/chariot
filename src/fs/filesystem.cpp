#include <fs/filesystem.h>
#include <fs/file.h>

fs::filesystem::filesystem(void) {}

fs::filesystem::~filesystem(void) {
}

ref<fs::inode> fs::filesystem::open(string path, u32 flags) {
  fs::path p = path;
  return open(p, flags);
}
