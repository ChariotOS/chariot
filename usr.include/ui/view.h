#pragma once

#include <ck/func.h>
#include <ck/intrusive_list.h>
#include <ck/object.h>
#include <ck/option.h>
#include <gfx/rect.h>
#include <gfx/scribe.h>
#include <ui/event.h>

namespace ui {

  // used for things like paddings or margins
  template <typename T>
  struct edges {
    T left = 0, right = 0, top = 0, bottom = 0;
  };

  // fwd decl
  class window;


  enum class layout : uint8_t { column, row };

  enum class size_policy : uint8_t {
    fixed,
    calc,
  };



  /*
   * A view defines a generic object that can be rendered in a view stack.
   * The base class defines a generic 'event' method that passes an event
   * down the stack to every view element
   */
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
    virtual bool mouse_event(ui::mouse_event &) { return false; };

    // tell the view to repaint
    virtual void repaint(void);


    /*
     * called when the view has been reflowed (layout recalculated and it
     * changed position, size, etc)
     */
    virtual void reflowed(void) {}


    /* relative positions relative to the parent */
    inline auto left() { return m_rel.left(); }
    inline auto right() { return m_rel.right(); }
    inline auto top() { return m_rel.top(); }
    inline auto bottom() { return m_rel.bottom(); }

    /*
     * Return the window for this view
     */
    inline ui::window *window() const { return m_window; }

    /*
     * Return a pointer to the view which owns this view as a child
     */
    inline ui::view *parent() const { return m_parent; }


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
      ui::view *v = new T(forward<Args>(args)...);

      m_children.append(*v);

      v->m_parent = this;
      v->m_window = m_window;
      return *(T *)v;
    }


    template <typename Fn>
    void each_child(Fn cb) {
      for (auto &v : m_children) cb(v);
    }


    inline void set_size(int w, int h) {
      m_rel.w = w;
      m_rel.h = h;
    }

    inline void set_pos(int x, int y) {
      m_rel.x = x;
      m_rel.y = y;
    }

    inline gfx::rect absolute_rect(void) {
      if (m_parent != NULL) {
        gfx::rect p = parent()->absolute_rect();
        gfx::rect r = m_rel;
        r.x += p.x;
        r.y += p.y;
        return r;
      }
      return m_rel;
    }


    // get a scribe for this view's bounding box
    gfx::scribe get_scribe(void);

    // invalidate *just* this view
    void invalidate(void);

    // cause the view to reflow
    void do_reflow(void);


    inline auto &margin(void) { return m_margin; }


    inline auto &padding(void) { return m_padding; }

    /*
inline auto get_bg(void) { return m_background; }
inline void set_bg(ck::option<uint32_t> c) { m_background = c; }
    */


    inline auto get_pref_size(void) {
      return gfx::rect(0, 0, m_pref_width, m_pref_height);
    }

    inline void set_pref_size(int w, int h) {
      m_pref_width = w;
      m_pref_height = h;
    }


    inline auto get_width_policy(void) const { return m_width_policy; }
    inline auto get_height_policy(void) const { return m_height_policy; }
    inline auto set_width_policy(ui::size_policy s) { m_width_policy = s; }
    inline auto set_height_policy(ui::size_policy s) { m_height_policy = s; }


    inline auto get_layout(void) const { return m_layout; }
    inline void set_layout(ui::layout l) { m_layout = l; }


   protected:
    friend ui::window;
    /*
     * Because views are meant to be nested ad infinitum, they hold information
     * representing their size within the *parent* view, not the overall window.
     * This makes layout calculations and whatnot much easier
     */
    gfx::rect m_rel;


    /*
     * Is this view visible?
     */
    bool m_visible = true;

    /// the background color
    // ck::option<uint32_t> m_background;


    ui::edges<int> m_margin;
    ui::edges<int> m_padding;

    uint32_t m_bordercolor = 0;
    uint32_t m_bodersize = 0;


    int m_pref_width;
    int m_pref_height;

    // by default, calculate width and height
    ui::size_policy m_width_policy = ui::size_policy::calc;
    ui::size_policy m_height_policy = ui::size_policy::calc;

    // default to a column layout
    ui::layout m_layout = ui::layout::column;

    /*
     * these two entries are mutually exclusive
     */
    ui::window *m_window;
    ui::view *m_parent;

    /* intrusive node for child list counting */
    ck::intrusive_list_node m_child_node;

    /*
     * A list of all the children in this view
     */
    ck::intrusive_list<ui::view, &ui::view::m_child_node> m_children;
  };
}  // namespace ui
