#ifndef __FILE_H__
#define __FILE_H__

#include <ptr.h>
#include <string.h>
#include <vec.h>
#include <dev/device.h>

namespace fs {


enum class file_type : u8 {
  unknown = 0,
  fifo,
  char_dev,
  dir,
  block_dev,
  file,
  symlink,
  unix_socket,
};

struct file_metadata {
  off_t size = 0;
  file_type type;
  u32 mode = 0;
  u32 uid = 0;
  u32 gid = 0;
  u32 link_count = 0;
  time_t atime = 0;
  time_t ctime = 0;
  time_t mtime = 0;
  time_t dtime = 0;
  u32 block_count = 0;
  u32 block_size = 0;
  // major/minor for block and char device types
  dev_t dev_info = {0, 0};
};

// fwd decl
class filedesc;

class file : public refcounted<file> {
 public:
   virtual ~file() = 0;
  // read and write interface
  virtual ssize_t read(filedesc &, void *, size_t) = 0;
  virtual ssize_t write(filedesc &, void *, size_t) = 0;

  // general interface for
  virtual file_metadata metadata(void) = 0;

  virtual inline bool is_inode() const { return false; }
  virtual inline bool is_fifo() const { return false; }
  virtual inline bool is_dev() const { return false; }

  virtual inline bool is_blk_dev() const { return false; }
  virtual inline bool is_char_dev() const { return false; }
};

class path {
 public:
  path(string);

  inline const string &operator[](u32 i) { return parts[i]; }

  // how many elements are in the path
  inline u32 len(void) { return parts.size(); }

  inline bool is_root(void) { return m_is_root; }

  string to_string(void);

 private:
  void parse(string &);
  bool m_is_root = false;

  vec<string> parts;
};
}  // namespace fs

#endif
