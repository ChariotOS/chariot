#pragma once


#include <types.h>

namespace fpu {
  // the FPU capabilities on this current processor
  struct fpu_caps {
    bool sse = false;
    bool xsave = false;
  };

  extern struct fpu_caps caps;

  uint16_t get_x87_status(void);
  void set_x87_ctrl(uint16_t val);
  bool has_x87(void);
  void enable_x87(void);

  void clear_x87_excp(void);

  void init();

};  // namespace fpu
