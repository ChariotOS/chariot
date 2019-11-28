#include <fs.h>
#include <errno.h>

ref<fs::filedesc> fs::filedesc::create(struct fs::inode *f, int flags) {
  // fail if f is null
  if (!f) return nullptr;

  // otherwise construct
  return make_ref<fs::filedesc>(f, flags);
}

fs::filedesc::filedesc(struct fs::inode *f, int flags) : m_file(f) {
  // register that the fd has access to the inode
  m_file->fd_open();
  m_offset = 0;
}

fs::filedesc::~filedesc(void) {
  m_file->fd_close();
}

off_t fs::filedesc::seek(off_t offset, int whence) {
  // TODO: check if the file is actually seekable


  off_t new_off;

  switch (whence) {
    case SEEK_SET:
      new_off = offset;
      break;
    case SEEK_CUR:
      new_off = m_offset + offset;
      break;
    case SEEK_END:
      new_off = m_file->size;
      break;
    default:
      new_off = whence + offset;
      break;
  }

  if (new_off < 0) return -EINVAL;

  // printk("new_off = %d, old = %d\n", new_off, m_offset);
  m_offset = new_off;
  return m_offset;
}


ssize_t fs::filedesc::read(void *dst, ssize_t len) {
  if (!m_file) {
    return -1;
  }

  return m_file->read(*this, dst, len);
}


ssize_t fs::filedesc::write(void *dst, ssize_t len) {
  if (!m_file) {
    return -1;
  }

  return m_file->write(*this, dst, len);
}
