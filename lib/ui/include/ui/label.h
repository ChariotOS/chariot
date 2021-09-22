#pragma once


#include <ui/view.h>

namespace ui {

  // Display a bit of text
  class label : public ui::view {
   public:
    label(ck::string content, ui::TextAlign align = ui::TextAlign::Center);
    virtual ~label(void);
    virtual void paint_event(gfx::scribe &s);

    const ck::string &content(void) const { return m_content; }
    void set_content(const ck::string &s);

    VIEW_RENDER_ATTRIBUTE(bool, autosize, false);

    inline bool is_autosize(void) const { return m_autosize; }
    void set_autosize(bool a);


   private:
    void size_to_fit(void);  ///< make sure there is enough width for the content of the label
    void wrap_text(void);

    ck::string m_content;
    ui::TextAlign m_align;
  };
};  // namespace ui


inline ck::ref<ui::label> label(ck::string content, ui::TextAlign align = ui::TextAlign::Center) {
  return ui::make<ui::label>(content, align);
}