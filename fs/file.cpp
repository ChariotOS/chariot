#include <dev/driver.h>
#include <errno.h>
#include <fs.h>
#include <fcntl.h>

ref<fs::file> fs::bdev_to_file(fs::blkdev *bdev) {
  fs::inode *ino = new fs::inode(T_BLK, fs::DUMMY_SB /* TODO */);

  ino->major = bdev->dev.major();
  ino->minor = bdev->dev.minor();

  dev::populate_inode_device(*ino);


  dev::populate_inode_device(*ino);

	string name = "/dev/";
	name += bdev->name;

  return fs::file::create(ino, name, FDIR_READ | FDIR_WRITE);
}

ref<fs::file> fs::file::create(struct fs::inode *f, string path, int flags) {
  // fail if f is null
  if (!f) return nullptr;
  // otherwise construct
  auto n = make_ref<fs::file>(f, flags);
  n->path = move(path);
  return move(n);
}

fs::file::file(struct fs::inode *f, int flags) : ino(f) {
  m_offset = 0;

  // register that the fd has access to the inode
  if (ino != nullptr) {
    geti(ino);
    auto ops = fops();
    int o_res = 0;
    if (ops && ops->open) o_res = ops->open(*this);
    if (o_res != 0) {
			m_error = o_res;
      fs::inode::release(ino);
      ino = NULL;
    }

  }

	if (ino != NULL) {
		if (flags & O_APPEND) {
			printk("append\n");
		}
	}
}

fs::file::~file(void) {
  if (ino != nullptr) {
    /*
    // close the fileriptor
    ino->close(*this);
    */

    auto ops = fops();
    if (ops && ops->close) ops->close(*this);
    fs::inode::release(ino);
    ino = nullptr;
  }
}

fs::file_operations *fs::file::fops(void) {
  if (!ino) return NULL;
  return ino->fops;
}

off_t fs::file::seek(off_t offset, int whence) {
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
      new_off = ino->size;
      break;
    default:
      new_off = whence + offset;
      break;
  }

  if (new_off < 0) return -EINVAL;

  fs::file_operations *ops = fops();
  if (ops && ops->seek) {
    int res = ops->seek(*this, m_offset, new_off);
    if (res != 0) return res;
  }

  m_offset = new_off;
  return m_offset;
}

ssize_t fs::file::read(void *dst, ssize_t len) {
  if (!ino) return -ENOENT;
  fs::file_operations *ops = fops();
  if (ops && ops->read) return ops->read(*this, (char *)dst, len);
  return -EINVAL;
}

ssize_t fs::file::write(void *data, ssize_t len) {
  if (!ino) return -ENOENT;
  fs::file_operations *ops = fops();
  if (ops && ops->write) return ops->write(*this, (char *)data, len);
  return -EINVAL;
}

int fs::file::ioctl(int cmd, unsigned long arg) {
  if (!ino) return -ENOENT;
  fs::file_operations *ops = fops();
  if (ops && ops->ioctl) return ops->ioctl(*this, cmd, arg);
  return -EINVAL;
}

