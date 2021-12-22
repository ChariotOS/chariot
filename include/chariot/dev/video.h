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
    virtual int get_mode(gvi_video_mode &mode);
    virtual int set_mode(const gvi_video_mode &mode);
    virtual uint32_t *get_framebuffer(void);
    virtual int flush_fb(void);

    static void register_driver(VideoDevice *vdev);

    virtual int on_open(void) { return 0; /* allow */ }
    virtual void on_close(void) {}
  };
}  // namespace dev
