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


  enum class direction : uint8_t { vertical, horizontal };

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
    // core mouse events
    inline virtual void on_left_click(ui::mouse_event &) {}
    inline virtual void on_right_click(ui::mouse_event &) {}
    inline virtual void on_scroll(ui::mouse_event &) {}
    inline virtual void on_mouse_move(ui::mouse_event &) {}

    inline virtual void paint_event(void) {}


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
    inline ui::window *window() const { return m_window; }

    /*
     * Return a pointer to the view which owns this view as a child
     */
    inline ui::view *parent() const { return m_parent; }




    // get a scribe for this view's bounding box
    gfx::scribe get_scribe(void);
    void invalidate(void);
    void do_reflow(void);
    inline auto &margin(void) { return m_margin; }
    inline auto &padding(void) { return m_padding; }


    ui::size_policy get_width_policy(void) const { return m_width_policy; }
    ui::size_policy get_height_policy(void) const { return m_height_policy; }
    void set_width_policy(ui::size_policy s) { m_width_policy = s; }
    void set_height_policy(ui::size_policy s) { m_height_policy = s; }

    void set_size(ui::direction dir, int sz);
    void set_size(int w, int h);
    void set_pos(int x, int y);
    gfx::rect absolute_rect(void);


    /**
     * Get the size of the view along a certain axis
     */
    int size(ui::direction dir);

    void set_pos(ui::direction dir, int pos);
    ui::size_policy get_size_policy(ui::direction dir);

    void set_background(uint32_t bg);
    void remove_background();




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
      do_reflow();
      return *(T *)v;
    }


    template <typename Fn>
    inline void each_child(Fn cb) {
      for (auto &v : m_children) cb(v);
    }




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


    uint32_t m_background = 0xFFFFFF;


    ui::edges<int> m_margin;
    ui::edges<int> m_padding;

    uint32_t m_bordercolor = 0;
    uint32_t m_bodersize = 0;


    // by default, calculate width and height
    ui::size_policy m_width_policy = ui::size_policy::calc;
    ui::size_policy m_height_policy = ui::size_policy::calc;

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




  class stackview : public ui::view {
   public:
    inline auto get_layout(void) const { return m_layout; }
    inline void set_layout(ui::direction l) {
      if (l == m_layout) return;
      m_layout = l;
      // since we changed the layout, we gotta force a reflow
      do_reflow();
    }

    inline stackview(ui::direction l = ui::direction::vertical) : m_layout(l) {}
    virtual ~stackview();

    inline void set_spacing(uint32_t s) { m_spacing = s; }
    inline auto spacing(void) const { return m_spacing; }


   protected:
    virtual void reflow_impl(void);


   private:
    uint32_t m_spacing = 0;
    ui::direction m_layout;
  };

}  // namespace ui
