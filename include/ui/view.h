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
#include <ui/surface.h>

// #include <ui/internal/flex.h>

#define STYLE_AUTO NAN



namespace ui {


  // fwd decl
  class window;


  enum class Direction : uint8_t { Vertical, Horizontal };




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

  using edges = ui::base_edges<float>;



  enum FlexAlign { Auto = 0, Stretch, Center, Start, End, SpaceBetween, SpaceAround, SpaceEvenly };
  enum FlexPosition { Relative = 0, Absolute };
  enum FlexWrap { NoWrap = 0, Wrap, Reverse };
  enum FlexDirection { Row = 0, RowReverse, Column, ColumnReverse };


  class view; /* fwd decl */


  struct flex_layout {
    struct line {
      unsigned int child_begin;
      unsigned int child_end;
      float size;
    };


    bool wrap = false;
    bool reverse_main = false;   // whether main axis is reversed
    bool reverse_cross = false;  // whether cross axis is reversed (wrap only)
    bool vertical = true;
    float size_dim = 0.0;   // main axis parent size
    float align_dim = 0.0;  // cross axis parent size

    /* the following are offests into the frame structure */
    unsigned int frame_main_pos = 0;    // main axis position
    unsigned int frame_cross_pos = 1;   // cross axis position
    unsigned int frame_main_size = 2;   // main axis size
    unsigned int frame_cross_size = 3;  // cross axis size
    unsigned int *ordered_indices = NULL;

    // Set for each line layout.
    float line_dim = 0.0;        // the cross axis size
    float flex_dim = 0.0;        // the flexible part of the main axis size
    float extra_flex_dim = 0.0;  // sizes of flexible items
    float flex_grows = 0.0;
    float flex_shrinks = 0.0;
    float cross_pos = 0.0;  // cross axis position

    // Calculated layout lines - only tracked when needed:
    //   - if the root's align_content property isn't set to FLEX_ALIGN_START
    //   - or if any child item doesn't have a cross-axis size set
    bool need_lines = false;
    ck::vec<ui::flex_layout::line> lines;
    float lines_sizes;


    flex_layout();
    ~flex_layout(void);
    void init(ui::view &item, float width, float height);

    ui::view *child_at(ui::view *v, int i);
    void reset(void);
  };



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
    inline virtual void mouse_event(ui::mouse_event &) {}
    inline virtual void paint_event(void) {}
    inline virtual void on_keydown(ui::keydown_event &) {}
    inline virtual void on_keyup(ui::keyup_event &) {}
    inline virtual void on_focused(void) {}
    inline virtual void on_blur(void) {}
    inline virtual void mounted(void) {}


    inline virtual void flex_self_sizing(float &width, float &height) {}

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


    inline auto relative(void) { return gfx::rect(left(), top(), width(), height()); }

    /* Is a point within the relative size */
    inline bool within_self(const gfx::point &p) { return gfx::rect(width(), height()).contains(p.x(), p.y()); }


    /*
     * Geometry relative to the parent view (or window)
     */
    inline int left() { return m_frame[0]; }
    inline int top() { return m_frame[1]; }
    inline int width() { return m_frame[2]; }
    inline int height() { return m_frame[3]; }

    inline int right() { return left() + width() - 1; }
    inline int bottom() { return top() + height() - 1; }

    /*
     * Return the window for this view
     */
    inline ui::surface *surface() {
      if (m_parent == NULL) {
        return m_surface;
      }
      return m_parent->surface();
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
     * Because the view tree is strict (views cannot live in multiple places,
     * and must have a parent or a window) new views are created by spawning
     * them within another view. This means that in order to create a view, you
     * *must* spawn it or it breaks assumptions about view ownership.
     *
     * It is safe to share this return value as long as you use it while the
     * parent view is in existence.
     */
    template <typename T, typename... Args>
    inline T &spawn(Args &&...args) {
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

#undef FLEX_ATTRIBUTE
#define FLEX_ATTRIBUTE(name, type, def)                         \
  inline const type &get_flex_##name(void) { return m_##name; } \
  inline void set_flex_##name(type val) { m_##name = val; };
#include <ui/internal/view_flex_attribs.h>
#undef FLEX_ATTRIBUTE




    // ask the view to do a layout (on itself)
    // returns if it was successful or not.
    bool layout(void);
    // layout with a width and height
    void layout(float width, float height);
    void layout_items(unsigned int child_begin, unsigned int child_end, unsigned int children_count,
                      struct flex_layout &layout);
    ui::FlexAlign child_align();


    bool log_layouts = false;

   public:
    float m_font_size = NAN;
    ck::ref<gfx::font> m_font = nullptr;


    friend ui::window;
    friend struct flex_layout;

    // bit flags
    bool m_visible = true;

    /*
     * these two entries are mutually exclusive
     */
    ui::surface *m_surface = NULL;
    ui::view *m_parent = NULL;

    /* A list of the children owned by this view */
    ck::vec<ck::unique_ptr<ui::view>> m_children;


    /*
     * Each view has their own flex_item, which is tied into the ui/internal/flex.c code
     * to do flexbox calculation
     */
   public:
    // struct flex_item *m_fi = NULL;


#define FLEX_ATTRIBUTE(name, type, def) type m_##name = def;
#include <ui/internal/view_flex_attribs.h>
    float m_frame[4];
    bool m_should_order_children = false;
  };


}  // namespace ui
