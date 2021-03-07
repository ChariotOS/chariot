#include <asm.h>
#include <dev/driver.h>
#include <errno.h>
#include <fs.h>
#include <mem.h>
#include <module.h>
#include <printk.h>


// TODO: remove
struct fs::superblock fs::DUMMY_SB;

using namespace fs;

#define for_in_ll(it, ll) for (auto *it = ll; it != nullptr; it = it->next)



fs::inode::inode(int type, fs::superblock &sb) : type(type), sb(sb) {
  fs::superblock::get(&sb);
  switch (type) {
    case T_DIR:
      // zero out the directory specific info
      dir.mountpoint = nullptr;  // where the directory is mounted to
      dir.entries = nullptr;
      dir.name = nullptr;
      break;

    case T_FILE:
      break;

    case T_CHAR:
    case T_BLK:
      break;

    default:
      break;
      // printk("unhandled inode constructor: %d\n", type);
  }
}

static void destruct_dir(struct inode *ino) {
  assert(ino->type == T_DIR);

  if (ino->dir.name) {
    free((void *)ino->dir.name);
    ino->dir.name = NULL;
  }

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
  // printk("INODE DESTRUCT %d\n", ino);
  if (fops && fops->destroy) fops->destroy(*this);

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
      // printk("unhandled inode destructor: %d\n", type);
      break;
  }

  fs::superblock::put(&sb);
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

  if (dops == NULL) panic("dir_ops null in get_direntry_ino despite being a directory\n");
  if (dops->lookup == NULL)
    panic("dir_ops->lookup null in get_direntry_ino despite being a directory\n");

  // otherwise attempt to resolve that entry
  ent->ino = dops->lookup(*this, ent->name.get());
  if (ent->ino != NULL) fs::inode::acquire(ent->ino);

  if (ent->ino->type == T_DIR) {
    ent->ino->set_name(ent->name);
  }

  return ent->ino;
}

int fs::inode::poll(fs::file &f, int events, poll_table &pt) {
  if (fops && fops->poll) {
    return fops->poll(f, events, pt);
  }
  return 0;
}

int fs::inode::add_mount(const char *name, struct fs::inode *guest) {
  for_in_ll(ent, dir.entries) {
    if (ent->name == name) {
      if (ent->mount_shadow != NULL) {
        return -EEXIST;
      }
      ent->mount_shadow = guest;
      return 0;
    }
  }
  return -ENOENT;
}

struct inode *fs::inode::get_direntry_nolock(const char *name) {
  assert(type == T_DIR);
  // special case ".." in the root of a mountpoint
  if (!strcmp(name, "..")) {
    if (dir.mountpoint != NULL) {
      return dir.mountpoint;
    }
  }

  for_in_ll(ent, dir.entries) {
    if (!strcmp(ent->name.get(), name)) {
      return get_direntry_ino(ent);
    }
  }
  return nullptr;
}

struct inode *fs::inode::get_direntry(const char *name) {
  assert(type == T_DIR);
  struct inode *ino = NULL;
  // lock.lock();
  ino = get_direntry_nolock(name);
  // lock.unlock();
  return ino;  // nothing found!
}

int fs::inode::register_direntry(string name, int enttype, int nr, struct inode *ino) {
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
  ent->nr = nr;
  ent->ino = ino;
  ent->type = enttype;

  ent->next = dir.entries;
  if (dir.entries != NULL) dir.entries->prev = ent;
  dir.entries = ent;

  lock.unlock();

  // do this after.
  if (ino != NULL && ino != this) {
    fs::inode::acquire(ent->ino);
  }
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
        /*
        if (ent->ino->dops->unlink(*ent->ino, ent->name.get()) != 0) {
                return -1;
        }
        */
      }
      if (ent->prev != NULL) ent->prev->next = ent->next;
      if (ent->next != NULL) ent->next->prev = ent->prev;
      if (ent == dir.entries) dir.entries = ent->next;

      if (ent->ino != NULL && ent->ino != this) fs::inode::release(ent->ino);
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

int fs::inode::set_name(const string &s) {
  if (type != T_DIR) {
    return -ENOTDIR;
  }
  if (dir.name != NULL) return 0;

  auto name = (char *)malloc(s.size() + 1);
  memcpy(name, s.get(), s.size() + 1);
  dir.name = name;
  return 0;
}

void fs::inode::walk_direntries(func<bool(const string &, struct inode *)> func) {
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

struct fs::inode *fs::inode::acquire(struct inode *in) {
  assert(in != NULL);
  in->lock.lock();
  in->rc++;
  // printk("rc = %d\n", in->rc);
  in->lock.unlock();
  return in;
}

int fs::inode::release(struct inode *in) {
  assert(in != NULL);
  in->lock.lock();
  in->rc--;
  // printk(KERN_DEBUG "rc=%d\n", in->rc);
  if (in->rc == 0) {
    delete in;
  } else {
    in->lock.unlock();
  }
  return 0;
}

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
int fs::inode::stat(struct stat *buf) {
  if (buf != NULL) {
    buf->st_dev = -1;
    buf->st_ino = ino;
    int mode = this->mode & 0777;

    if (type == T_DIR) mode |= S_IFDIR;
    if (type == T_BLK) mode |= S_IFBLK;
    if (type == T_CHAR) mode |= S_IFCHR;
    if (type == T_FIFO) mode |= S_IFIFO;
    if (type == T_SYML) mode |= S_IFLNK;
    if (type == T_SOCK) mode |= S_IFSOCK;
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
