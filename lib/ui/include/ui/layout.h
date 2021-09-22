#pragma once

#include <ck/ptr.h>
#include <gfx/geom.h>
#include <ui/edges.h>

namespace ui {

  // fwd decl
  class view;

  class layout : public ck::refcounted<ui::layout> {
   public:
    virtual ~layout(void);

    void notify_adopted(ui::view &);
    void notify_disowned(ui::view &);

    virtual void run(ui::view &) = 0;
    virtual gfx::isize preferred_size(ui::view &) const = 0;


    const ui::edges &margins() const;
    const ui::edges &padding() const;


    int spacing() const { return m_spacing; }
    void set_spacing(int);

   protected:
    ui::view *m_owner;

    ui::edges m_margins;
    int m_spacing = 0;
  };
}  // namespace ui
