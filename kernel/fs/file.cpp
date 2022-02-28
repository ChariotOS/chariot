#include <dev/driver.h>
#include <errno.h>
#include <fcntl.h>
#include <fs.h>

ck::ref<fs::File> fs::File::create(ck::ref<fs::Node> f, ck::string path, int flags) {
  // fail if f is null
  if (!f) return nullptr;
  // otherwise construct
  auto n = ck::make_ref<fs::File>(f, flags);
  n->path = move(path);
  return move(n);
}

fs::File::File(ck::ref<fs::Node> f, int flags) : ino(f) {
  // TODO: flags!
  m_offset = 0;

  // register that the fd has access to the inode
  if (ino != nullptr) {
    f->open(*this);
  }

  if (ino != nullptr) {
    if (flags & O_APPEND) {
      seek(0, SEEK_END);
    }
  }
}

fs::File::~File(void) {
  if (ino != nullptr) {
    ino->close(*this);
  }
}


off_t fs::File::seek(off_t offset, int whence) {
  // TODO: check if the file is actually seekable
  //
  if (!ino) return -EINVAL;

  off_t new_off;

  switch (whence) {
    case SEEK_SET:
      new_off = offset;
      break;
    case SEEK_CUR:
      new_off = m_offset + offset;
      break;
    case SEEK_END: {
      struct stat s;
      ino->stat(&s);
      new_off = s.st_size;
      break;
    }
    default:
      new_off = whence + offset;
      break;
  }

  if (new_off < 0) return -EINVAL;

  int res = ino->seek_check(*this, m_offset, new_off);
  if (res != 0) return res;

  m_offset = new_off;
  return m_offset;
}

ssize_t fs::File::read(void *dst, ssize_t len) {
  if (!ino) return -ENOENT;
  return ino->read(*this, (char *)dst, len);
}

ssize_t fs::File::write(void *data, ssize_t len) {
  if (!ino) return -ENOENT;
  return ino->write(*this, (char *)data, len);
}

int fs::File::ioctl(int cmd, unsigned long arg) {
  if (!ino) return -ENOENT;
  return ino->ioctl(*this, cmd, arg);
}
