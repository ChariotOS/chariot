#include <errno.h>
#include <fs/file.h>
#include <fs/vfs.h>
#include <map.h>

u64 vfs::qualified_inode_number(const fs::filesystem &f, u32 inode) {
  return (u64)f.id() << 32 | inode;
}

vfs::mountpoint::mountpoint(unique_ptr<fs::filesystem> fs, fs::vnoderef host) {
  m_fs = move(fs);
  m_host = move(host);
}

fs::vnoderef vfs::mountpoint::host() const {
  return m_host;
}


fs::vnoderef vfs::mountpoint::guest() {
  return m_fs->get_root_inode();
}

vfs::mountpoint::mountpoint() {}

bool vfs::mountpoint::operator==(const mountpoint &other) {
  // two mounts are equal if their hosts are the same and their filesystems are
  // the same
  return other.m_host->index() == m_host->index() &&
         m_fs.get() == other.m_fs.get();
}

vfs::vfs() { panic("DO NOT CONSTRUCT A VFS INSTANCE\n"); }

static vfs::mountpoint root_mountpoint;

static map<u64, unique_ptr<vfs::mountpoint>> mount_points;

int vfs::mount(unique_ptr<fs::filesystem> fs, fs::vnoderef host) {
  // check that the host is valid
  if (!host) return -ENOENT;

  // check that the filesystem is valid
  if (!fs) return -ENOENT;

  auto mid = qualified_inode_number(*fs.get(), host->index());

  // if there is already a fs mounted here, fail
  if (mount_points.contains(mid)) return -EBUSY;

  // now we can actually add the mount
  auto mpt = make_unique<mountpoint>(move(fs), host);
  mount_points.set(mid, move(mpt));

  return 0;
}

int vfs::mount_root(unique_ptr<fs::filesystem> fs) {
  // TODO: check for errors :)
  root_mountpoint = mountpoint(move(fs), fs->get_root_inode());
  return 0;
}

fs::vnoderef vfs::get_mount_at(u64 inode) {
  if (mount_points.contains(inode)) {
    return mount_points.get(inode)->guest();
  } else {
    return {};
  }
}

fs::vnoderef vfs::open(string spath, int opts, int mode) {
  fs::path p = spath;

  fs::vnoderef curr = {};

  if (!p.is_root()) {
    // TODO: use the current processes' CWD as root, and walk from there.
    panic("fs::vfs::open should always receive a rooted path. '%s'\n",
          spath.get());
  } else {
    curr = root_mountpoint.m_fs->get_root_inode();
  }

  // start out assuming we've found the file, for `open("/", ...);` cases
  bool found = true;

  for (int i = 0; i < p.len(); i++) {
    // if we are on the last directory of the path
    bool last = i == p.len() - 1;

    auto &targ = p[i];

    bool contained = false;
    // TODO: swap this out for a find_dir_entry function or something
    curr->walk_dir([&](const string &fname, fs::vnoderef vn) -> bool {
      if (fname == targ) {
        curr = vn;
        contained = true;
        return false;
      }
      return true;
    });
    if (!contained) {
      if (last && (opts & O_CREAT)) {
        // TODO: check for O_CREAT and last
        printk("NOT FOUND AT LAST, WOULD CREATE\n");
      }
      return {};
    }
  }

  // if the file wasnt found, return a null inode
  if (!found) return {};

  return curr;
}
