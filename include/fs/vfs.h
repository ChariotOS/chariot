#pragma once

#include <fs/filesystem.h>
#include <ptr.h>
#include <string.h>

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_EXCL 0200
#define O_NOCTTY 0400
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_DIRECTORY 00200000
#define O_NOFOLLOW 00400000
#define O_CLOEXEC 02000000
#define O_NOFOLLOW_NOERROR 0x4000000

/**
 *
 * The virtual file system is the system that allows filesystems to be mounted
 * recursively in the directory tree. It is the main API for generalized
 * filesystem access
 */

class vfs {
 public:
  class mountpoint {
   public:
    mountpoint(unique_ptr<fs::filesystem>, fs::vnoderef host);
    mountpoint();

    fs::inode host() const;
    fs::inode guest();

    string absolute_path() const;

    bool operator==(const mountpoint &);

   protected:
    friend class vfs;

    unique_ptr<fs::filesystem> m_fs = {};
    fs::vnoderef m_host = {};
  };

  // mount a filesystem into a host vnode
  static int mount(unique_ptr<fs::filesystem>, fs::vnoderef host);

  static int mount_root(unique_ptr<fs::filesystem>);

  static fs::vnoderef open(string path, int opts, int mode = 0000);

 private:
  vfs();  // private constructor. use static methods
};

