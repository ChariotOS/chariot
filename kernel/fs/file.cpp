#include <dev/driver.h>
#include <errno.h>
#include <fcntl.h>
#include <fs.h>

ck::ref<fs::File> fs::bdev_to_file(fs::BlockDevice *bdev) {
  fs::Node *ino = new fs::Node(T_BLK, fs::DUMMY_SB /* TODO */);

  ino->major = bdev->dev.major();
  ino->minor = bdev->dev.minor();

  dev::populate_inode_device(*ino);


  dev::populate_inode_device(*ino);

  ck::string name = "/dev/";
  name += bdev->name;

  return fs::File::create(ino, name, FDIR_READ | FDIR_WRITE);
}

ck::ref<fs::File> fs::File::create(struct fs::Node *f, ck::string path, int flags) {
  // fail if f is null
  if (!f) return nullptr;
  // otherwise construct
  auto n = ck::make_ref<fs::File>(f, flags);
  n->path = move(path);
  return move(n);
}

fs::File::File(struct fs::Node *f, int flags) : ino(f) {
  m_offset = 0;

  // register that the fd has access to the inode
  if (ino != nullptr) {
    geti(ino);
    auto ops = fops();
    int o_res = 0;
    if (ops && ops->open) o_res = ops->open(*this);
    if (o_res != 0) {
      m_error = o_res;
      fs::Node::release(ino);
      ino = NULL;
    }
  }

  if (ino != NULL) {
    if (flags & O_APPEND) {
      seek(0, SEEK_END);
    }
  }
}

fs::File::~File(void) {
  if (ino != nullptr) {
    // close the file
    auto ops = fops();
    if (ops && ops->close) ops->close(*this);
    fs::Node::release(ino);
    ino = nullptr;
  }
}

fs::FileOperations *fs::File::fops(void) {
  if (!ino) return NULL;
  return ino->fops;
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
    case SEEK_END:
      new_off = ino->size;
      break;
    default:
      new_off = whence + offset;
      break;
  }

  if (new_off < 0) return -EINVAL;

  fs::FileOperations *ops = fops();
  if (ops && ops->seek) {
    int res = ops->seek(*this, m_offset, new_off);
    if (res != 0) return res;
  }

  m_offset = new_off;
  return m_offset;
}

ssize_t fs::File::read(void *dst, ssize_t len) {
  if (!ino) return -ENOENT;
  fs::FileOperations *ops = fops();
  if (ops && ops->read) return ops->read(*this, (char *)dst, len);
  return -EINVAL;
}

ssize_t fs::File::write(void *data, ssize_t len) {
  if (!ino) return -ENOENT;
  fs::FileOperations *ops = fops();
  if (ops && ops->write) return ops->write(*this, (char *)data, len);
  return -EINVAL;
}

int fs::File::ioctl(int cmd, unsigned long arg) {
  if (!ino) return -ENOENT;
  fs::FileOperations *ops = fops();
  if (ops && ops->ioctl) return ops->ioctl(*this, cmd, arg);
  return -EINVAL;
}
