#pragma once

#include <ck/func.h>
#include <ck/intrusive_list.h>
#include <ck/macros.h>
#include <ck/object.h>
#include <ck/option.h>
#include <gfx/rect.h>
#include <gfx/scribe.h>
#include <ui/event.h>
#include <ui/internal/flex.h>

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


#define VIEW_RENDER_ATTRIBUTE(type, name, val)        \
 protected:                                           \
  type name = val;                                    \
                                                      \
 public:                                              \
  inline type get_##name(void) { return this->name; } \
                                                      \
 public:                                              \
  inline void set_##name(const type &v) { this->name = v; }




// #define SIZE_AUTO (-

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


    // make this widget the focused one
    void set_focused(void);

    // distribute a mouse event to children or the parent if it's better suited
    // for it
    void dispatch_mouse_event(ui::mouse_event &ev);

    // ask the view to repaint at the next possible time
    void repaint(void);


    /*
     * called when the view has been reflowed (layout recalculated and it
     * changed position, size, etc)
     */
    virtual void reflowed(void) {}


    /*
     * Geometry relative to the parent view (or window)
     */
    inline auto left() { return m_rel.left(); }
    inline auto right() { return m_rel.right(); }
    inline auto top() { return m_rel.top(); }
    inline auto bottom() { return m_rel.bottom(); }
    inline auto width() { return m_rel.w; }
    inline auto height() { return m_rel.h; }

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


    void set_size(ui::Direction dir, int sz);
    void set_size(int w, int h);
    void set_pos(int x, int y);
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

    inline void add(ui::view *v) {
      v->m_parent = this;
      m_children.push(ck::unique_ptr(v));
      do_reflow();
      v->mounted();
    }



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



    VIEW_RENDER_ATTRIBUTE(uint32_t, background, 0xFFFFFF);
    VIEW_RENDER_ATTRIBUTE(uint32_t, foreground, 0xFFFFFF);
    VIEW_RENDER_ATTRIBUTE(uint32_t, bordercolor, 0xFFFFFF);
    VIEW_RENDER_ATTRIBUTE(uint32_t, bordersize, 0);

    VIEW_RENDER_ATTRIBUTE(ui::SizePolicy, width_policy, ui::SizePolicy::Calculate);
    VIEW_RENDER_ATTRIBUTE(ui::SizePolicy, height_policy, ui::SizePolicy::Calculate);

    VIEW_RENDER_ATTRIBUTE(ui::edges, margin, {});
    VIEW_RENDER_ATTRIBUTE(ui::edges, padding, {});


   protected:
    // implemented by the subclass
    virtual void reflow_impl() {}

    friend ui::window;
    /*
     * Because views are meant to be nested ad infinitum, they hold information
     * representing their size within the *parent* view, not the overall window.
     * This makes layout calculations and whatnot much easier
     */
    gfx::rect m_rel;

    // bit flags
    bool m_visible = true;


    bool m_use_bg = true;

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
  };




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

}  // namespace ui
