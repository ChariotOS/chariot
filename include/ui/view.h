#pragma once

#include <ck/func.h>
#include <gfx/direction.h>
#include <ck/intrusive_list.h>
#include <ck/macros.h>
#include <ck/object.h>
#include <ck/option.h>
#include <gfx/font.h>
#include <gfx/geom.h>
#include <gfx/rect.h>
#include <gfx/scribe.h>
#include <math.h>
#include <ui/event.h>
#include <ui/surface.h>
#include <ui/layout.h>
#include <ui/edges.h>
#include <std.h>

// #include <ui/internal/flex.h>

#define STYLE_AUTO NAN



namespace ui {


  // fwd decl
  class window;

  enum FlexAlign { Auto = 0, Stretch, Center, Start, End, SpaceBetween, SpaceAround, SpaceEvenly };
  enum FlexPosition { Relative = 0, Absolute };
  enum FlexWrap { NoWrap = 0, Wrap, Reverse };
  enum FlexDirection { Row = 0, RowReverse, Column, ColumnReverse };


  class view; /* fwd decl */


  // a structure which you can use to initialize the styling of a view
  struct style {
    ck::option<uint32_t> background;
    ck::option<uint32_t> foreground;
    ck::option<uint32_t> bordercolor;
    ck::option<uint32_t> bordersize;
    ck::ref<ui::layout> layout;
    ck::option<ui::edges> margins;
    ck::option<ui::edges> padding;
  };


#define VIEW_RENDER_ATTRIBUTE(type, name, val)        \
 protected:                                           \
  type m_##name = val;                                \
                                                      \
 public:                                              \
  inline type name(void) { return this->m_##name; }   \
                                                      \
 public:                                              \
  inline void set_##name(const ck::option<type> &v) { \
    this->m_##name = v;                               \
    update();                                         \
  }

#define OPTIONAL_VIEW_RENDER_ATTRIBUTE(type, name, val) \
 protected:                                             \
  ck::option<type> o_##name = {};                       \
                                                        \
 public:                                                \
  inline type get_##name(void) {                        \
    if (this->o_##name) {                               \
      return this->o_##name.unwrap();                   \
    } else {                                            \
      return val;                                       \
    }                                                   \
  }                                                     \
                                                        \
 public:                                                \
  inline void set_##name(const ck::option<type> &v) {   \
    this->o_##name = v;                                 \
    update();                                           \
  }                                                     \
  inline void clear_##name(void) {                      \
    this->o_##name = None;                              \
    update();                                           \
  }                                                     \
  inline bool has_##name(void) {                        \
    return this->o_##name.has_value();                  \
    ;                                                   \
    update();                                           \
  }




  /*
   * A view defines a generic object that can be rendered in a view stack.
   * The base class defines a generic 'event' method that passes an event
   * down the stack to every view element
   */

  class view : public ck::refcounted<ui::view> {
    CK_NONCOPYABLE(view);
    CK_MAKE_NONMOVABLE(view);

   public:
    view();

    // a helper function to create a view with some children
    // view(ck::vec<ck::ref<ui::view>> children);
    view(std::initializer_list<ui::view *> children);

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
    virtual void mouse_event(ui::mouse_event &) {}
    virtual void paint_event(gfx::scribe &) {}
    virtual void on_keydown(ui::keydown_event &) {}
    virtual void on_keyup(ui::keyup_event &) {}
    virtual void on_focused(void) {}
    virtual void on_blur(void) {}
    virtual void mounted(void) {}
    virtual void custom_layout(void) {}


    inline virtual void flex_self_sizing(float &width, float &height) {}

    // make this view the focused one
    void set_focused(void);

    // distribute a mouse event to children or the parent if it's better suited
    // for it
    void dispatch_mouse_event(ui::mouse_event &ev);

    // ask the view to repaint at the next possible time
    void repaint(gfx::rect &relative_dirty_region);


    /*
     * called when the view has been reflowed (layout recalculated and it
     * changed position, size, etc). No painting is to occur here.
     */
    virtual void reflowed(void) {}


    inline auto relative(void) { return gfx::rect(left(), top(), width(), height()); }

    /* Is a point within the relative size */
    inline bool within_self(const gfx::point &p) {
      return gfx::rect(width(), height()).contains(p.x(), p.y());
    }



    inline ui::layout *layout(void) { return m_layout.get(); }
    void set_layout(ck::ref<ui::layout>);

    template <class T, class... Args>
    inline T &set_layout(Args &&...args) {
      auto l = ck::make_ref<T>(forward<Args>(args)...);
      set_layout(l);
      return *l;
    }

    void run_layout(int depth = 0);

    gfx::isize min_size() const { return m_min_size; }
    void set_min_size(const gfx::isize &sz) { m_min_size = sz; }
    void set_min_size(int width, int height) { set_min_size({width, height}); }

    int min_width() const { return m_min_size.width(); }
    int min_height() const { return m_min_size.height(); }
    void set_min_width(int width) { set_min_size(width, min_height()); }
    void set_min_height(int height) { set_min_size(min_width(), height); }

    gfx::isize max_size() const { return m_max_size; }
    void set_max_size(const gfx::isize &sz) { m_max_size = sz; }
    void set_max_size(int width, int height) { set_max_size({width, height}); }

    int max_width() const { return m_max_size.width(); }
    int max_height() const { return m_max_size.height(); }
    void set_max_width(int width) { set_max_size(width, max_height()); }
    void set_max_height(int height) { set_max_size(max_width(), height); }

    void set_fixed_size(const gfx::isize &size) {
      set_min_size(size);
      set_max_size(size);
    }

    void set_width(int w) { m_relative_rect.w = w; }
    void set_height(int h) { m_relative_rect.h = h; }


    void set_fixed_size(int width, int height) { set_fixed_size({width, height}); }

    void set_fixed_width(int width) {
      set_min_width(width);
      set_max_width(width);
    }

    void set_fixed_height(int height) {
      set_min_height(height);
      set_max_height(height);
    }

    void set_shrink_to_fit(bool b) {
      if (m_shrink_to_fit == b) return;
      m_shrink_to_fit = b;
      surface()->schedule_reflow();
    }
    bool is_shrink_to_fit() const { return m_shrink_to_fit; }

    // relative positions within the parent
    inline int left() const { return m_relative_rect.x; }
    inline int top() const { return m_relative_rect.y; }
    inline int width() const { return m_relative_rect.w; }
    inline int height() const { return m_relative_rect.h; }
    inline int right() const { return left() + width() - 1; }
    inline int bottom() const { return top() + height() - 1; }
    // self-sized rectangle
    gfx::rect rect() const { return {0, 0, width(), height()}; }
    gfx::isize size() const { return {width(), height()}; }
    // set the location of this view within the parent view
    void set_relative_rect(gfx::rect &r);
    // interior padding
    const ui::edges &padding(void) const { return m_padding; }
    void set_padding(const ui::edges &p) {
      m_padding = p;
      update_layout();
    }
    void set_padding(int p) { set_padding({p, p, p, p}); }
    void set_padding(int tb, int lr) { set_padding(base_edges(tb, lr)); }
    // exterior margins
    const ui::edges &margins(void) const { return m_margins; }
    void set_margins(const ui::edges &p) {
      m_margins = p;
      update_layout();
    }
    void set_margins(int p) { set_margins({p, p, p, p}); }
    void set_margins(int tb, int lr) { set_margins(base_edges(tb, lr)); }
    // get the
    inline ui::surface *surface() const {
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
    void update(void);
    void update(gfx::rect);


    void update_layout(void);

    inline void set_border(uint32_t color, uint32_t size) {
      set_bordercolor(color);
      set_bordersize(size);
    }

    gfx::rect absolute_rect(void);

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

    ui::view &add(ck::ref<ui::view> v);

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

    OPTIONAL_VIEW_RENDER_ATTRIBUTE(uint32_t, background, 0xFFFFFF);
    OPTIONAL_VIEW_RENDER_ATTRIBUTE(uint32_t, foreground, 0x000000);
    OPTIONAL_VIEW_RENDER_ATTRIBUTE(uint32_t, bordercolor, 0x000000);
    OPTIONAL_VIEW_RENDER_ATTRIBUTE(uint32_t, bordersize, 0);

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


    bool log_layouts = false;

   public:
    float m_font_size = NAN;
    ck::ref<gfx::font> m_font = nullptr;


    friend ui::window;


    /* A list of the children owned by this view */
    ck::vec<ck::ref<ui::view>> m_children;

    void dump_hierarchy(int depth = 0);




    void load_style(const ui::style &cfg);

   private:
    // void needs_relayout() { m_needs_relayout = true; }
    /*
     * these two entries are mutually exclusive
     */
    ui::surface *m_surface = NULL;
    ui::view *m_parent = NULL;


    // bit flags
    bool m_visible = true;
    bool m_enabled = true;
    // can this view shrink to fit within the parent's layout?
    bool m_shrink_to_fit = false;

    ck::ref<ui::layout> m_layout;

    // where in the parent is this view located
    gfx::rect m_relative_rect;

    ui::edges m_margins;
    ui::edges m_padding;
    // minimum and maximum size constraints on this view. -1 means none
    gfx::isize m_min_size{-1, -1};
    gfx::isize m_max_size{-1, -1};
  };



  template <typename T>
  ck::ref<T> styled(ck::ref<T> v, const ui::style &st) {
    v->load_style(st);
    return v;
  }

  template <typename T>
  ck::ref<T> with_children(ck::ref<T> v, std::initializer_list<ck::ref<ui::view>> children) {
    for (auto &c : children) {
      v->add(c);
    }
    return v;
  }

  // a convenient `make` function which produces a ref
  template <typename Type, typename... Args>
  inline ck::ref<Type> make(Args &&...args) {
    return ck::ref<Type>(new Type(::forward<Args>(args)...));
  }

  // a convenient `make` function which produces a ref
  // template <typename Type, typename... Args>
  // inline ck::ref<Type> make(const ui::cfg &cfg, Args &&...args) {
  //   auto v = ck::ref<Type>(new Type(::forward<Args>(args)...));
  //   v->load_config(cfg);
  //   return v;
  // }

  // a convenient `make` function which produces a ref
  template <typename Type>
  inline ck::ref<Type> make(std::initializer_list<ck::ref<ui::view>> children) {
    auto v = ck::ref<Type>(new Type());
    return ui::with_children(v, children);
  }

  // a convenient `make` function which produces a ref
  template <typename Type, typename... Args>
  inline ck::ref<Type> make(std::initializer_list<ck::ref<ui::view>> children, Args &&...args) {
    auto v = ck::ref<Type>(new Type(::forward<Args>(args)...));
    return ui::with_children(v, children);
  }

  template <typename Type, typename... Args>
  inline ck::ref<Type> make(
      ui::style style, std::initializer_list<ck::ref<ui::view>> children, Args &&...args) {
    auto v = ck::ref<Type>(new Type(::forward<Args>(args)...));
    return styled(ui::with_children(v, children), style);
  }

  template <typename Type, typename... Args>
  inline ck::ref<Type> make(ui::style style, Args &&...args) {
    auto v = ck::ref<Type>(new Type(::forward<Args>(args)...));
    return styled(v, style);
  }



#define STYLED(v, ...) ui::styled(v, {__VA_ARGS__})

  // a convenient `make` function which produces a ref
  // template <typename Type, typename... Args>
  // inline ck::ref<Type> make(
  //     const ui::cfg &cfg, std::initializer_list<ck::ref<ui::view>> children, Args &&...args) {
  //   auto v = ck::ref<Type>(new Type(::forward<Args>(args)...));
  //   for (auto &c : children) {
  //     v->add(c);
  //   }
  //   v->load_config(cfg);
  //   return v;
  // }




}  // namespace ui