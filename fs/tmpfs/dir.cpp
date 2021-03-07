#include <errno.h>
#include "tmpfs.h"




// create a file in the directory
static int tmpfs_create(fs::inode &ino, const char *name, struct fs::file_ownership &own) {
  if (ino.type != T_DIR) panic("tmpfs_create on non-dir\n");


  auto &sb = tmp::getsb(ino);

  // make a normal file
  auto ent = sb.create_inode(T_FILE);
  ent->gid = own.gid;
  ent->uid = own.uid;
  ent->mode = own.mode;
  ino.register_direntry(name, ENT_MEM, ent->ino, ent);
  return 0;
}
// create a directory in a dir
static int tmpfs_mkdir(fs::inode &ino, const char *name, struct fs::file_ownership &own) {
  fs::inode *dir = tmp::getsb(ino).create_inode(T_DIR);
  dir->gid = own.gid;
  dir->uid = own.uid;
  dir->mode = own.mode;
  dir->set_name(name);
  ino.register_direntry(name, ENT_MEM, dir->ino, dir);

  // make ./
  dir->register_direntry(".", ENT_MEM, dir->ino, dir);
  // make ../
  dir->register_direntry("..", ENT_MEM, ino.ino, &ino);

  return 0;
}
// remove a file from a directory
static int tmpfs_unlink(fs::inode &node, const char *entry) {
  if (node.type != T_DIR) panic("tmpfs_unlink on non-dir\n");

  return node.remove_direntry(entry);
}

// lookup an inode by name in a file
static struct fs::inode *tmpfs_lookup(fs::inode &node, const char *needle) {
  if (node.type != T_DIR) panic("tmpfs_lookup on non-dir\n");

  // walk the linked list to get the inode num
  for (auto *it = node.dir.entries; it != NULL; it = it->next) {
    if (!strcmp(it->name.get(), needle)) {
      return it->ino;
    }
  }
  return NULL;
}
// create a device node with a major and minor number
static int tmpfs_mknod(fs::inode &, const char *name, struct fs::file_ownership &, int major,
                       int minor) {
  UNIMPL();
  return -ENOTIMPL;
}


fs::dir_operations tmp::dops = {

    .create = tmpfs_create,
    .mkdir = tmpfs_mkdir,
    .unlink = tmpfs_unlink,
    .lookup = tmpfs_lookup,
    .mknod = tmpfs_mknod,
};
