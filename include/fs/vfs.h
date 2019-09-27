#pragma once

#include <fs/filesystem.h>
#include <ptr.h>
#include <string.h>


/**
 *
 * The virtual file system allows filesystems to be mounted recursively in the
 * directory tree. It is the main API for generalized filesystem access
 */

class vfs {
 public:
  class mountpoint {
   public:
    mountpoint(unique_ptr<fs::filesystem>, fs::vnoderef host);
    mountpoint();

    fs::vnoderef host() const;
    fs::vnoderef guest();

    string absolute_path() const;

    bool operator==(const mountpoint &);

   protected:
    friend class vfs;

    unique_ptr<fs::filesystem> m_fs = {};
    fs::vnoderef m_host = {};
  };

  // mount a filesystem into a host vnode
  static int mount(unique_ptr<fs::filesystem>, fs::vnoderef host);
  static int mount(unique_ptr<fs::filesystem>, string path);

  static int mount_root(unique_ptr<fs::filesystem>);

  static fs::vnoderef open(string path, int opts = 0, int mode = 0000);

  // get the vnode located at the qualified inode
  static fs::vnoderef get_mount_at(u64 qual_inode);


  static fs::vnoderef get_mount_host(u64 guest_inode);

  static u64 qualified_inode_number(const fs::filesystem &f, u32 inode);

 private:
  vfs();  // private constructor. use static methods
};

