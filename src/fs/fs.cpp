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

    default:
      printk("unhandled inode constructor: %d\n", type);
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
  return ent->ino = resolve_direntry(ent->name);
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
  switch (type) {
    case T_DIR:
      destruct_dir(this);
      break;

    default:
      printk("unhandled inode destructor: %d\n", type);
  }
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

      if (ent->ino != NULL) delete ent->ino;
      // TODO: notify the vfs that the mount was deleted?
      if (ent->mount_shadow != NULL) delete ent->mount_shadow;
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
  lock.lock();

  for_in_ll(ent, dir.entries) {
    auto ino = get_direntry_ino(ent);
    if (!func(ent->name, ino)) {
      break;
    }
  }

  lock.unlock();
}

