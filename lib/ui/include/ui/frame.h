#pragma once

#include <ui/view.h>
#include <ck/timer.h>

namespace ui {

  class frame : public ui::view {
   public:
    frame();
    virtual ~frame(void);


    int frame_thickness(void) const { return m_thickness; }
    void set_frame_thickness(int thickness);

   protected:
   private:
    int m_thickness = 0;
  };

}  // namespace ui
