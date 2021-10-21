#include <asm.h>
#include <dev/driver.h>
#include <errno.h>
#include <fs.h>
#include <mem.h>
#include <module.h>
#include <printk.h>


// TODO: remove
ck::ref<fs::SuperBlock> fs::DUMMY_SB = ck::make_ref<fs::SuperBlock>();

using namespace fs;

#define for_in_ll(it, ll) for (auto *it = ll; it != nullptr; it = it->next)



fs::Node::Node(int type, ck::ref<fs::SuperBlock> sb) : type(type), sb(sb) {
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

static void destruct_dir(struct Node *ino) {
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
    delete ent;
  }
}

fs::Node::~Node() {
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
}


ck::ref<fs::Node> fs::Node::get_direntry_ino(struct DirectoryEntry *ent) {
  assert(type == T_DIR);
  // no lock, internal api
  // if it was shadowed by a mount, return that
  if (ent->mount_shadow) {
    return ent->mount_shadow;
  }

  // if the stored inode is not nullptr, return that
  if (ent->ino) {
    return ent->ino;
  }

  if (dops == nullptr) panic("dir_ops null in get_direntry_ino despite being a directory\n");
  if (dops->lookup == NULL) panic("dir_ops->lookup null in get_direntry_ino despite being a directory\n");

  // otherwise attempt to resolve that entry
  ent->ino = dops->lookup(*this, ent->name.get());

  if (ent->ino->type == T_DIR) {
    ent->ino->set_name(ent->name);
  }

  return ent->ino;
}

int fs::Node::poll(fs::File &f, int events, poll_table &pt) {
  if (fops && fops->poll) {
    return fops->poll(f, events, pt);
  }
  return 0;
}

int fs::Node::add_mount(const char *name, ck::ref<fs::Node> guest) {
  for_in_ll(ent, dir.entries) {
    if (ent->name == name) {
      if (ent->mount_shadow) {
        return -EEXIST;
      }
      ent->mount_shadow = guest;
      return 0;
    }
  }
  return -ENOENT;
}

ck::ref<fs::Node> fs::Node::get_direntry_nolock(const char *name) {
  assert(type == T_DIR);
  // special case ".." in the root of a mountpoint
  if (!strcmp(name, "..")) {
    if (dir.mountpoint) {
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

ck::ref<fs::Node> fs::Node::get_direntry(const char *name) {
  assert(type == T_DIR);
  ck::ref<fs::Node> ino = get_direntry_nolock(name);
  // lock.unlock();
  return ino;  // nothing found!
}

int fs::Node::register_direntry(ck::string name, int enttype, int nr, ck::ref<fs::Node> ino) {
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
  auto ent = new fs::DirectoryEntry;
  ent->name = move(name);
  ent->nr = nr;
  ent->ino = ino;
  ent->type = enttype;

  ent->next = dir.entries;
  if (dir.entries != NULL) dir.entries->prev = ent;
  dir.entries = ent;

  lock.unlock();

  return 0;
}

int fs::Node::remove_direntry(ck::string name) {
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
      if (ent->prev != nullptr) ent->prev->next = ent->next;
      if (ent->next != nullptr) ent->next->prev = ent->prev;
      if (ent == dir.entries) dir.entries = ent->next;

      delete ent;
      lock.unlock();
      return 0;
    }
  }

  lock.unlock();
  return -ENOENT;
}

int fs::Node::set_name(const ck::string &s) {
  if (type != T_DIR) {
    return -ENOTDIR;
  }
  if (dir.name != NULL) return 0;

  auto name = (char *)malloc(s.size() + 1);
  memcpy(name, s.get(), s.size() + 1);
  dir.name = name;
  return 0;
}

void fs::Node::walk_direntries(ck::func<bool(const ck::string &, ck::ref<fs::Node>)> func) {
  assert(type == T_DIR);

  for_in_ll(ent, dir.entries) {
    auto ino = get_direntry_ino(ent);
    if (!func(ent->name, ino)) {
      break;
    }
  }
}

ck::vec<ck::string> fs::Node::direntries(void) {
  assert(type == T_DIR);

  ck::vec<ck::string> e;
  for_in_ll(ent, dir.entries) { e.push(ent->name); }
  return e;
}


#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
int fs::Node::stat(struct stat *buf) {
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
