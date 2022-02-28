#include <dev/video.h>
#include <errno.h>
#include <dev/driver.h>
#include <phys.h>
#include <mm.h>

dev::VideoDevice::~VideoDevice() { /* TODO */
}

// default
int dev::VideoDevice::get_mode(gvi_video_mode &) { return -ENOTIMPL; }

int dev::VideoDevice::flush_fb(void) { return -ENOTIMPL; }

// default
int dev::VideoDevice::set_mode(const gvi_video_mode &) { return -ENOTIMPL; }

uint32_t *dev::VideoDevice::get_framebuffer(void) { return NULL; }

static bool initialized = false;
static ck::vec<dev::VideoDevice *> m_VideoDrivers;

static dev::VideoDevice *get_vdev(int minor) {
  if (minor >= 0 && minor < m_VideoDrivers.size()) {
    return m_VideoDrivers[minor];
  }
  return nullptr;
}

struct VideoVMObject final : public mm::VMObject {
  dev::VideoDevice &vdev;
  VideoVMObject(dev::VideoDevice &vdev, size_t npages) : VMObject(npages), vdev(vdev) {}

  virtual ~VideoVMObject(void){};

  virtual ck::ref<mm::Page> get_shared(off_t n) override {
    auto fb = (unsigned long)vdev.get_framebuffer();
    if (fb == 0) return nullptr;
    auto p = mm::Page::create(fb + (n * PGSIZE));

    return p;
  }
};

ck::ref<mm::VMObject> dev::VideoDevice::mmap(fs::File &file, size_t npages, int prot, int flags, off_t off) {
  gvi_video_mode mode;
  if (this->get_mode(mode) != 0) return nullptr;

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

  return ck::make_ref<VideoVMObject>(*this, npages);
}



int dev::VideoDevice::ioctl(fs::File &file, unsigned int cmd, off_t arg) {
  if (cmd == GVI_IDENTIFY) {
    return GVI_IDENTIFY_MAGIC;
  }


  if (cmd == GVI_GET_MODE) {
    if (!VALIDATE_WR((void *)arg, sizeof(gvi_video_mode))) return -EINVAL;
    struct gvi_video_mode mode;
    int ret = this->get_mode(mode);
    *(gvi_video_mode *)arg = mode;
    return ret;
  }

  if (cmd == GVI_FLUSH_FB) {
    return flush_fb();
  }

  return -ENOTIMPL;
}

void dev::VideoDevice::register_instance(void) {
  int minor = m_VideoDrivers.size();
  ck::string name = ck::string::format("video%d", minor);
  m_VideoDrivers.push(this);
  this->bind(name);
}



// static struct dev::DriverInfo gvi_driver_info { .name = "Generic Video Interface", .type = DRIVER_CHAR, .major = MAJOR_VIDEO, };
