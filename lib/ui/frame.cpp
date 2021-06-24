#include <ui/frame.h>
#include <math.h>
#include "sys/sysbind.h"
#include <sys/sysbind.h>

ui::frame::frame(void) { set_frame_thickness(0); }

ui::frame::~frame(void) {}


void ui::frame::set_frame_thickness(int thickness) {
  m_thickness = thickness;
  set_padding(frame_thickness());
}
