#pragma once
#include <gfx/geom.h>

namespace ui {

  // used for things like paddings or margins
  template <typename T>
  struct base_edges {
    T left = 0, right = 0, top = 0, bottom = 0;

    inline base_edges(const base_edges<T>& o) {
      left = o.left;
      right = o.right;
      top = o.top;
      bottom = o.bottom;
    }

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

    inline T total_for(gfx::Direction dir) const {
      if (dir == gfx::Direction::Horizontal) return left + right;
      if (dir == gfx::Direction::Vertical) return top + bottom;
      return 0;
    }

    inline T total_for_secondary(gfx::Direction dir) const {
      if (dir == gfx::Direction::Vertical) return left + right;
      if (dir == gfx::Direction::Horizontal) return top + bottom;
      return 0;
    }


    inline T base_for(gfx::Direction dir) const {
      if (dir == gfx::Direction::Horizontal) return left;
      if (dir == gfx::Direction::Vertical) return top;
      return 0;
    }
  };

  using edges = ui::base_edges<int>;

};  // namespace ui
