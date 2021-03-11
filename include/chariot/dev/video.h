#pragma once

#include <gvi.h>
#include <types.h>

namespace dev {
  /* Generic wrapper class for a video device */
  class video_device {
   public:
    virtual ~video_device();
    virtual int get_mode(gvi_video_mode &mode);
    virtual int set_mode(const gvi_video_mode &mode);
    virtual uint32_t *get_framebuffer(void);
		virtual int flush_fb(void);

    static void register_device(video_device *vdev);
  };
}  // namespace dev
