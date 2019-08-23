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

namespace fs {


class vfs {
 public:
  class mount {
   public:
    mount(unique_ptr<fs::filesystem>);

    fs::inode host() const;
    fs::inode guest() const;

    string absolute_path() const;

   private:
    // The directory which the filesystem is mounted into
    fs::inode m_host;
    // the guest root inode
    fs::inode m_guest;
  };


 private:
  vfs();  // private constructor. use static methods
};

};  // namespace fs
