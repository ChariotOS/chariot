#pragma once

#include <ck/func.h>
#include <ck/intrusive_list.h>
#include <ck/macros.h>
#include <ck/object.h>
#include <ck/option.h>
#include <gfx/font.h>
#include <gfx/rect.h>
#include <gfx/scribe.h>
#include <math.h>
#include <ui/event.h>
#include <ui/internal/flex.h>

#define STYLE_AUTO NAN

namespace ui {


  // fwd decl
  class window;


  enum class Direction : uint8_t { Vertical, Horizontal };

  enum class SizePolicy : uint8_t {
    Fixed,
    Calculate,
  };



  // used for things like paddings or margins
  template <typename T>
  struct base_edges {
    T left = 0, right = 0, top = 0, bottom = 0;


    inline base_edges() {}
    inline base_edges(T v) { left = right = top = bottom = v; }
    inline base_edges(T tb, T lr) {
      top = bottom = tb;
      left = right = lr;
    }


    inline base_edges(T t, T r, T b, T l) {
      top = t;
      right = r;
      bottom = b;
      left = l;
    }

    inline T total_for(ui::Direction dir) {
      if (dir == ui::Direction::Horizontal) return left + right;
      if (dir == ui::Direction::Vertical) return top + bottom;
      return 0;
    }


    inline T base_for(ui::Direction dir) {
      if (dir == ui::Direction::Horizontal) return left;
      if (dir == ui::Direction::Vertical) return top;
      return 0;
    }
  };

  using edges = ui::base_edges<int>;


  /*
   * A view defines a generic object that can be rendered in a view stack.
   * The base class defines a generic 'event' method that passes an event
   * down the stack to every view element
   */


#define VIEW_RENDER_ATTRIBUTE(type, name, val) \
 protected:                                    \
  ck::option<type> name = {};                  \
                                               \
 public:                                       \
  inline type get_##name(void) {               \
    if (this->name) {                          \
      return this->name.unwrap();              \
    }                                          \
    if (parent() != NULL) {                    \
      return parent()->get_##name();           \
    }                                          \
    return val;                                \
  }                                            \
                                               \
 public:                                       \
  inline void set_##name(const ck::option<type> &v) { this->name = v; }



  class view {
    CK_NONCOPYABLE(view);
    CK_MAKE_NONMOVABLE(view);

   public:
    view();
    virtual ~view();
    /*
     * Called on each `view` recursively. This method is optionally overloaded,
     * but the default implementation should be enough for most situations. If
     * this method returns `true`, the event has been "absorbed" by this view or
     * a sub view
     */
    virtual bool event(ui::event &);


    // default handlers
    // core mouse events
    inline virtual void on_left_click(ui::mouse_event &) {}
    inline virtual void on_right_click(ui::mouse_event &) {}
    inline virtual void on_scroll(ui::mouse_event &) {}
    inline virtual void on_mouse_move(ui::mouse_event &) {}
    inline virtual void paint_event(void) {}
    inline virtual void on_keydown(ui::keydown_event &) {}
    inline virtual void on_keyup(ui::keyup_event &) {}
    inline virtual void on_focused(void) {}
    inline virtual void on_blur(void) {}
    inline virtual void mounted(void) {}


		inline virtual void flex_self_sizing(float &width, float &height) {
		}

    // make this widget the focused one
    void set_focused(void);

    // distribute a mouse event to children or the parent if it's better suited
    // for it
    void dispatch_mouse_event(ui::mouse_event &ev);

    // ask the view to repaint at the next possible time
    void repaint(bool do_invaldiate = true);


    /*
     * called when the view has been reflowed (layout recalculated and it
     * changed position, size, etc)
     */
    virtual void reflowed(void) {}


    inline auto relative(void) {
      return gfx::rect(flex_item_get_frame_x(m_fi), flex_item_get_frame_y(m_fi), flex_item_get_frame_width(m_fi),
                       flex_item_get_frame_height(m_fi));
    }


    /*
     * Geometry relative to the parent view (or window)
     */
    inline int left() { return flex_item_get_frame_x(m_fi); }
    inline int top() { return flex_item_get_frame_y(m_fi); }
    inline int width() { return flex_item_get_frame_width(m_fi); }
    inline int height() { return flex_item_get_frame_height(m_fi); }

    inline int right() { return left() + width() - 1; }
    inline int bottom() { return top() + height() - 1; }

    /*
     * Return the window for this view
     */
    inline ui::window *window() {
      if (m_parent == NULL) {
        return m_window;
      }
      return m_parent->window();
    }

    /*
     * Return a pointer to the view which owns this view as a child
     */
    inline ui::view *parent() const { return m_parent; }




    // get a scribe for this view's bounding box
    gfx::scribe get_scribe(void);
    void invalidate(void);
    void do_reflow(void);

    inline void set_border(uint32_t color, uint32_t size) {
      set_bordercolor(color);
      set_bordersize(size);
    }


    void set_size(ui::Direction dir, float sz);
    void set_size(float w, float h);
    // void set_pos(int x, int y);
    gfx::rect absolute_rect(void);
    // area inside padding
    gfx::rect padded_area(void);


    /**
     * Get the size of the view along a certain axis
     */
    int size(ui::Direction dir);

    void set_pos(ui::Direction dir, int pos);
    ui::SizePolicy get_size_policy(ui::Direction dir);



    /**
     * Because the view tree is strict (views cannot live in multiple places,
     * and must have a parent or a window) new views are created by spawning
     * them within another view. This means that in order to create a view, you
     * *must* spawn it or it breaks assumptions about view ownership.
     *
     * It is safe to share this return value as long as you use it while the
     * parent view is in existence.
     */
    template <typename T, typename... Args>
    inline T &spawn(Args &&... args) {
      auto v = new T(forward<Args>(args)...);
      auto &r = *v;
      add(move(v));
      return *v;
    }

    void add(ui::view *v);



    friend inline ui::view &operator<<(ui::view &lhs, ui::view *rhs) {
      lhs.add(rhs);
      return lhs;
    }


    void clear(void);



    template <typename Fn>
    inline void each_child(Fn cb) {
      for (auto &v : m_children) {
        cb(*v);
      }
    }




    void mark_dirty(void);



    VIEW_RENDER_ATTRIBUTE(uint32_t, background, 0xFFFFFF);
    VIEW_RENDER_ATTRIBUTE(uint32_t, foreground, 0x000000);
    VIEW_RENDER_ATTRIBUTE(uint32_t, bordercolor, 0x000000);
    VIEW_RENDER_ATTRIBUTE(uint32_t, bordersize, 0);

    VIEW_RENDER_ATTRIBUTE(ui::SizePolicy, width_policy, ui::SizePolicy::Calculate);
    VIEW_RENDER_ATTRIBUTE(ui::SizePolicy, height_policy, ui::SizePolicy::Calculate);

    inline auto get_font(void) {
      if (!m_font) {
        if (parent() == NULL) return gfx::font::get_default();
        return parent()->get_font();
      }
      return m_font;
    }

    inline void set_font(const char *name) { m_font = gfx::font::get(name); }


    inline void set_font_size(float sz) { m_font_size = sz; }

    inline int get_font_size(void) {
      if (isnan(m_font_size)) {
        if (parent() == NULL) return 12;  // default fon
        return parent()->get_font_size();
      }
      return m_font_size;
    }

    // VIEW_RENDER_ATTRIBUTE(ui::edges, margin, {});
    // VIEW_RENDER_ATTRIBUTE(ui::edges, padding, {});


#undef FLEX_ATTRIBUTE
#define FLEX_ATTRIBUTE(name, type, def)                                    \
  inline type get_flex_##name(void) { return flex_item_get_##name(m_fi); } \
  inline void set_flex_##name(type val) { flex_item_set_##name(m_fi, val); };
#include <ui/internal/flex_attributes.h>
#undef FLEX_ATTRIBUTE


   protected:
    float m_font_size = NAN;
    ck::ref<gfx::font> m_font = nullptr;


    friend ui::window;

    // bit flags
    bool m_visible = true;

    /*
     * these two entries are mutually exclusive
     */
    ui::window *m_window = NULL;
    ui::view *m_parent = NULL;

    /* intrusive node for child list counting */
    // ck::intrusive_list_node m_child_node;

    /*
     * A list of all the children in this view
     */
    // ck::intrusive_list<ui::view, &ui::view::m_child_node> m_children;
    ck::vec<ck::unique_ptr<ui::view>> m_children;

    /*
     * Each view has their own flex_item, which is tied into the ui/internal/flex.c code
     * to do flexbox calculation
     */
    struct flex_item *m_fi = NULL;
  };




#if 0
  class stackview : public ui::view {
   public:
    inline auto get_layout(void) const { return m_layout; }
    inline void set_layout(ui::Direction l) {
      if (l == m_layout) return;
      m_layout = l;
      // since we changed the layout, we gotta force a reflow
      do_reflow();
    }

    inline stackview(ui::Direction l = ui::Direction::Vertical) : m_layout(l) {}
    virtual ~stackview();

    inline void set_spacing(uint32_t s) { m_spacing = s; }
    inline auto spacing(void) const { return m_spacing; }


   protected:
    virtual void reflow_impl(void);


   private:
    uint32_t m_spacing = 0;
    ui::Direction m_layout;
  };
#endif

}  // namespace ui
