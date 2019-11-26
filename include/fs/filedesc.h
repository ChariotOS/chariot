#pragma once

#include <fs/file.h>
#include <fs/vnode.h>
#include <ptr.h>
#include <types.h>

#define FDIR_READ 1
#define FDIR_WRITE 2

#define SEEK_SET (-1)
#define SEEK_CUR (-2)
#define SEEK_END (-3)

namespace fs {
class filedesc : public refcounted<filedesc> {
 public:
  // must construct file descriptors via these factory funcs
  static ref<filedesc> create(ref<file>, int flags = FDIR_READ | FDIR_WRITE);

  /*
   * seek - change the offset
   * set the file position to the offset + whence if whence is not equal to
   * either SEEK_SET, SEEK_CUR, or SEEK_END. (defaults to start)
   */
  off_t seek(off_t offset, int whence = SEEK_SET);
  ssize_t read(void*, ssize_t);
  ssize_t write(void* data, ssize_t);

  int close();

  file_metadata metadata(void);

  inline off_t offset(void) { return m_offset; }



 public:
  filedesc(ref<file>, int flags);

  ref<file> m_file;

  off_t m_offset = 0;
};

}  // namespace fs
