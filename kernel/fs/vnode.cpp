#include <fs/filesystem.h>
#include <fs/vfs.h>
#include <fs/vnode.h>

fs::vnode::vnode(u32 index) : m_index(index) {}
fs::vnode::~vnode(void) {}

// so we dont construct these all the time in walk_dir
static string dot = ".";
static string dotdot = "..";

bool fs::vnode::walk_dir(func<bool(const string &, ref<vnode>)> cb) {
  return false;
}

