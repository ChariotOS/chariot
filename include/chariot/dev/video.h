#pragma once

#include <gvi.h>
#include <types.h>
#include <dev/driver.h>

namespace dev {
  /* Generic wrapper class for a video device */
  class VideoDevice : public dev::Device {
   public:
    using dev::Device::Device;
    virtual ~VideoDevice();


    // For children:
    virtual int get_mode(gvi_video_mode &mode);
    virtual int set_mode(const gvi_video_mode &mode);
    virtual uint32_t *get_framebuffer(void);
    virtual int flush_fb(void);

    void register_instance(void);

    // ^fs::Node
    ck::ref<mm::VMObject> mmap(fs::File &file, size_t npages, int prot, int flags, off_t off) override;
    int ioctl(fs::File &file, unsigned int cmd, off_t arg) override;
  };
}  // namespace dev
