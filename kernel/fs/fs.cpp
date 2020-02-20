#include <dev/driver.h>
#include <errno.h>
#include <fs.h>
#include <module.h>
#include <printk.h>

using namespace fs;

#define for_in_ll(it, ll) for (auto *it = ll; it != nullptr; it = it->next)

fs::inode::inode(int type) : type(type) {
  switch (type) {
    case T_DIR:
      // zero out the directory specific info
      dir.mountpoint = nullptr;  // where the directory is mounted to
      dir.entries = nullptr;
      break;

    case T_FILE:
      break;

    case T_CHAR:
    case T_BLK:
      break;

    default:
      printk("unhandled inode constructor: %d\n", type);
  }
}

static void destruct_dir(struct inode *ino) {
  assert(ino->type == T_DIR);

  for_in_ll(ent, ino->dir.entries) {
    // don't delete the backpointers and current directory pointers
    if (ent->name == "." || ent->name == "..") {
      continue;
    }
    if (ent->ino != NULL) {
      delete ent->ino;
    }
    delete ent;
  }
}

fs::inode::~inode() {
  printk("INODE DESTRUCT %d\n", ino);

  switch (type) {
    case T_DIR:
      destruct_dir(this);
      break;
    case T_FILE:
      break;

    case T_CHAR:
    case T_BLK:
      break;

    default:
      printk("unhandled inode destructor: %d\n", type);
  }
}

struct inode *fs::inode::get_direntry_ino(struct direntry *ent) {
  assert(type == T_DIR);
  // no lock, internal api
  // if it was shadowed by a mount, return that
  if (ent->mount_shadow != NULL) {
    return ent->mount_shadow;
  }

  // if the stored inode is not null, return that
  if (ent->ino != NULL) {
    return ent->ino;
  }

  // otherwise attempt to resolve that entry
  ent->ino = resolve_direntry(ent->name);

  if (ent->ino != NULL) fs::inode::acquire(ent->ino);

  return ent->ino;
}

struct inode *fs::inode::get_direntry_nolock(string &name) {
  assert(type == T_DIR);
  // special case ".." in the root of a mountpoint
  if (name == "..") {
    if (dir.mountpoint != NULL) {
      return dir.mountpoint;
    }
  }

  for_in_ll(ent, dir.entries) {
    if (ent->name == name) {
      return get_direntry_ino(ent);
    }
  }
  return nullptr;
}

struct inode *fs::inode::get_direntry(string &name) {
  assert(type == T_DIR);
  struct inode *ino = NULL;
  lock.lock();
  ino = get_direntry_nolock(name);
  lock.unlock();
  return ino;  // nothing found!
}

int fs::inode::register_direntry(string name, int enttype, struct inode *ino) {
  assert(type == T_DIR);
  lock.lock();

  // check that there isn't a directory entry by that name
  for_in_ll(ent, dir.entries) {
    if (ent->name == name) {
      // there already exists a directory by that name
      lock.unlock();
      return -EEXIST;
    }
  }

  auto ent = new fs::direntry;
  ent->name = move(name);
  ent->ino = ino;

  ent->type = enttype;

  ent->next = dir.entries;
  if (dir.entries != NULL) dir.entries->prev = ent;
  dir.entries = ent;

  if (ent->ino != NULL) {
    fs::inode::acquire(ent->ino);
  }

  lock.unlock();
  return 0;
}

int fs::inode::remove_direntry(string name) {
  assert(type == T_DIR);
  lock.lock();

  // walk the list and remove the entry
  for_in_ll(ent, dir.entries) {
    if (ent->name == name) {
      if (ent->type == ENT_RES) {
        // check with the filesystem implementation
        if (rm(ent->name) != 0) {
          return -1;
        }
      }
      if (ent->prev != NULL) ent->prev->next = ent->next;
      if (ent->next != NULL) ent->next->prev = ent->prev;
      if (ent == dir.entries) dir.entries = ent->next;

      if (ent->ino != NULL) fs::inode::release(ent->ino);
      // TODO: notify the vfs that the mount was deleted?
      if (ent->mount_shadow != NULL) fs::inode::release(ent->mount_shadow);
      delete ent;
      lock.unlock();
      return 0;
    }
  }

  lock.unlock();
  return -ENOENT;
}

struct inode *fs::inode::resolve_direntry(string &name) {
  panic("resolving direntry on 'super' fs::inode struct (not implemented)\n");
  return NULL;
}

int fs::inode::rm(string &name) {
  panic(
      "removing resident direntry on 'super' fs::inode doesn't make sense (not "
      "implemented)\n");
  return -1;
}

void fs::inode::walk_direntries(
    func<bool(const string &, struct inode *)> func) {
  assert(type == T_DIR);

  for_in_ll(ent, dir.entries) {
    auto ino = get_direntry_ino(ent);
    if (!func(ent->name, ino)) {
      break;
    }
  }
}


vec<string> fs::inode::direntries(void) {
  assert(type == T_DIR);

  vec<string> e;
  for_in_ll(ent, dir.entries) {
    e.push(ent->name);
  }
  return e;
}

ssize_t fs::inode::read(filedesc &fd, void *buf, size_t sz) {
  ssize_t k = -1;

  dev::driver_ops *driver = NULL;

  switch (type) {
    case T_BLK:
    case T_CHAR:
      driver = dev::get(major);
      if (driver != NULL && driver->read != NULL) {
        k = driver->read(fd, (char *)buf, sz);
      }
      break;

    case T_FILE:
      lock.lock();
      k = do_read(fd, buf, sz);
      lock.unlock();
      break;
  }
  return k;
}
ssize_t fs::inode::write(filedesc &fd, void *buf, size_t sz) {
  ssize_t k = -1;

  dev::driver_ops *driver = NULL;

  switch (type) {
    case T_BLK:
    case T_CHAR:
      driver = dev::get(major);
      if (driver != NULL && driver->write != NULL) {
        k = driver->write(fd, (char *)buf, sz);
      }
      break;

    case T_FILE:
      lock.lock();
      k = do_write(fd, buf, sz);
      lock.unlock();
      break;
  }
  return k;
}

// to be implemented by the filesystem
int fs::inode::touch(string name, int mode, fs::inode *&dst) {
  dst = NULL;
  return -EINVAL;
}

int fs::inode::open(filedesc &fd) {
  ssize_t k = 0;

  dev::driver_ops *driver = NULL;

  switch (type) {
    case T_BLK:
    case T_CHAR:
      driver = dev::get(major);
      if (driver != NULL && driver->open != NULL) {
        k = driver->open(fd);
      }
      break;

    // by default, open should succeed
    default:
      k = 0;
  }
  return k;
}

void fs::inode::close(filedesc &fd) {
  dev::driver_ops *driver = NULL;

  switch (type) {
    case T_BLK:
    case T_CHAR:
      driver = dev::get(major);
      if (driver != NULL && driver->close != NULL) {
        driver->close(fd);
      }
      break;
  }
}

ssize_t fs::inode::do_read(filedesc &fd, void *buf, size_t sz) { return -1; }
ssize_t fs::inode::do_write(filedesc &fd, void *buf, size_t sz) { return -1; }

int fs::inode::acquire(struct inode *in) {
  assert(in != NULL);
  in->lock.lock();
  // printk("acquire\n");
  in->rc++;
  in->lock.unlock();
  return 0;
}

int fs::inode::release(struct inode *in) {
  return 0;
  assert(in != NULL);
  in->lock.lock();
  // printk("release\n");
  in->rc--;
  if (in->rc == 0) {
    delete in;
  }
  in->lock.unlock();
  return 0;
}

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
int fs::inode::stat(struct stat *buf) {
  if (buf != NULL) {
    buf->st_dev = -1;
    buf->st_ino = ino;
    buf->st_mode = mode;
    buf->st_nlink = link_count;
    buf->st_uid = uid;
    buf->st_gid = gid;
    buf->st_rdev = -1;
    buf->st_size = size;

    // TODO?
    buf->st_blocks = round_up(size, 512) / 512;
    buf->st_blksize = 512;

    // TODO: time
    buf->st_atim = atime;
    buf->st_ctim = ctime;
    buf->st_mtim = mtime;
    return 0;
  }

  return -1;
}
