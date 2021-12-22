#include <dev/video.h>
#include <errno.h>
#include <dev/driver.h>
#include <errno.h>
#include <phys.h>
#include <mm.h>

dev::VideoDriver::~VideoDriver() { /* TODO */
}

// default
int dev::VideoDriver::get_mode(gvi_video_mode &) { return -ENOTIMPL; }

int dev::VideoDriver::flush_fb(void) { return -ENOTIMPL; }

// default
int dev::VideoDriver::set_mode(const gvi_video_mode &) { return -ENOTIMPL; }

uint32_t *dev::VideoDriver::get_framebuffer(void) { return NULL; }

static bool initialized = false;
static ck::vec<dev::VideoDriver *> m_VideoDrivers;

static dev::VideoDriver *get_vdev(int minor) {
  if (minor >= 0 && minor < m_VideoDrivers.size()) {
    return m_VideoDrivers[minor];
  }
  return nullptr;
}

struct gvi_vmobject final : public mm::VMObject {
  dev::VideoDriver &vdev;
  gvi_vmobject(dev::VideoDriver &vdev, size_t npages) : VMObject(npages), vdev(vdev) {}

  virtual ~gvi_vmobject(void){};

  virtual ck::ref<mm::Page> get_shared(off_t n) override {
    auto fb = (unsigned long)vdev.get_framebuffer();
    if (fb == 0) return nullptr;
    auto p = mm::Page::create(fb + (n * PGSIZE));

    // p->fset(PG_NOCACHE | PG_WRTHRU);

    return p;
  }
};

static ck::ref<mm::VMObject> gvi_mmap(fs::File &fd, size_t npages, int prot, int flags, off_t off) {
  auto *vdev = get_vdev(fd.ino->minor);
  if (vdev == NULL) return nullptr;

  gvi_video_mode mode;
  if (vdev->get_mode(mode) != 0) return nullptr;

  uint64_t size = mode.width * mode.height * sizeof(uint32_t);

  // XXX: this is invalid, should be asserted before here :^)
  if (off != 0) {
    printk(KERN_WARN "gvi: attempt to mmap at invalid offset (%d != 0)\n", off);
    return nullptr;
  }

  if (npages > NPAGES(size)) {
    printk(KERN_WARN "gvi: attempt to mmap too many pages (%d pixels)\n", (npages * 4096) / sizeof(uint32_t));
    return nullptr;
  }

  if (flags & MAP_PRIVATE) {
    printk(KERN_WARN "gvi: attempt to mmap with MAP_PRIVATE doesn't make sense :^)\n");
    return nullptr;
  }

  return ck::make_ref<gvi_vmobject>(*vdev, npages);
}

static int gvi_ioctl(fs::File &fd, unsigned int cmd, unsigned long arg) {
  auto *vdev = get_vdev(fd.ino->minor);
  if (vdev == NULL) return -EINVAL;
  if (cmd == GVI_IDENTIFY) {
    return GVI_IDENTIFY_MAGIC;
  }


  if (cmd == GVI_GET_MODE) {
    if (!VALIDATE_WR((void *)arg, sizeof(gvi_video_mode))) return -EINVAL;
    struct gvi_video_mode mode;
    int ret = vdev->get_mode(mode);
    *(gvi_video_mode *)arg = mode;
    return ret;
  }

  if (cmd == GVI_FLUSH_FB) {
    return vdev->flush_fb();
  }

  return -ENOTIMPL;
}


static int gvi_open(fs::File &fd) {
  auto *vdev = get_vdev(fd.ino->minor);
  if (vdev == NULL) return -EINVAL;
  return vdev->on_open();
}

static void gvi_close(fs::File &fd) {
  auto *vdev = get_vdev(fd.ino->minor);
  if (vdev == NULL) return;

  vdev->on_close();
  // reset_fb();
}


struct fs::FileOperations generic_VideoDriver_ops = {
    .ioctl = gvi_ioctl,
    .open = gvi_open,
    .close = gvi_close,
    .mmap = gvi_mmap,
};

static struct dev::DriverInfo gvi_driver_info {
  .name = "Generic Video Interface", .type = DRIVER_CHAR, .major = MAJOR_VIDEO, .char_ops = &generic_VideoDriver_ops,
};

void dev::VideoDriver::register_driver(dev::VideoDriver *vdev) {
  if (!initialized) {
    dev::register_driver(gvi_driver_info);
    initialized = true;
  }


  int minor = m_VideoDrivers.size();
  ck::string name = ck::string::format("video%d", minor);
  m_VideoDrivers.push(vdev);
  dev::register_name(gvi_driver_info, name, minor);
}
