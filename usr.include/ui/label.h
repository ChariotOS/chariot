#pragma once


#include <ui/view.h>

namespace ui {

  // Display a bit of text
  class label : public ui::view {
    ck::string m_contents;
    ui::TextAlign m_align;

   public:
    label(ck::string contents, ui::TextAlign align = ui::TextAlign::TopLeft);
    virtual ~label(void);

		void set_text(ck::string contents);

		virtual void flex_self_sizing(float &width, float &height);
    virtual void paint_event(void);
  };
};  // namespace ui

