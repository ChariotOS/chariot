#pragma once

#include <ck/ptr.h>
#include <ck/map.h>
#include <fs/Node.h>

namespace fs {
  // A `File` is an abstraction of all file-like objects
  // when they have been opened by a process. They are
  // basically just the backing structure referenced by
  // a filedescriptor from userspace.
  class File final : public ck::refcounted<File> {
    int m_error = 0;

   public:
    // must construct file descriptors via these factory funcs
    static ck::ref<File> create(ck::ref<fs::Node>, ck::string open_path, int flags = FDIR_READ | FDIR_WRITE);

    /*
     * seek - change the offset
     * set the file position to the offset + whence if whence is not equal to
     * either SEEK_SET, SEEK_CUR, or SEEK_END. (defaults to start)
     */
    off_t seek(off_t offset, int whence = SEEK_SET);
    ssize_t read(void *, ssize_t);
    ssize_t write(void *data, ssize_t);
    int ioctl(int cmd, unsigned long arg);
    int stat(struct stat *stat) { return ino->stat(stat); }
    int close();


    inline off_t offset(void) { return m_offset; }

    // fs::FileOperations *fops(void);

    inline int errorcode() { return m_error; };

    ~File(void);


    File(ck::ref<fs::Node>, int flags);
    inline File() : File(nullptr, 0) {}

    inline operator bool() { return ino != nullptr; }

    ck::ref<fs::Node> ino;
    ck::string path;
    off_t m_offset = 0;

    bool can_write = false;
    bool can_read = false;

    int pflags = 0;  // private flags. Also used to track ptmx id
  };
}  // namespace fs