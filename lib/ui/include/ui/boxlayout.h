#pragma once
#include <ui/layout.h>
#include <gfx/geom.h>
#include <ui/view.h>

namespace ui {

  class boxlayout : public ui::layout {
   public:
    virtual ~boxlayout() override {}


    gfx::Direction direction(void) const { return m_direction; }

    virtual void run(ui::view &) override;
    virtual gfx::isize preferred_size(ui::view &) const override;


   protected:
    explicit boxlayout(gfx::Direction dir) : m_direction(dir) {}

   private:
    int preferred_primary_size(ui::view &) const;
    int preferred_secondary_size(ui::view &) const;
    gfx::Direction m_direction;
  };


  // a box layout that is vertical
  class vboxlayout : public ui::boxlayout {
   public:
    explicit vboxlayout(void) : boxlayout(gfx::Direction::Vertical) {}

    virtual ~vboxlayout(void) override {}
  };


  // a box layout that is horizontal
  class hboxlayout : public ui::boxlayout {
   public:
    explicit hboxlayout(void) : boxlayout(gfx::Direction::Horizontal) {}

    virtual ~hboxlayout(void) override {}
  };




  // a view with a vertical box layout by default
  class vbox : public ui::view {
   public:
    inline vbox() { set_layout<ui::vboxlayout>(); }
    ~vbox() = default;
  };

  // a view with a horizontal box layout by default
  class hbox : public ui::view {
   public:
    inline hbox() { set_layout<ui::hboxlayout>(); }
    ~hbox() = default;
  };
};  // namespace ui
