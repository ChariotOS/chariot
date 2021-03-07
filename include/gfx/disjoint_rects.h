#pragma once
#include <ck/vec.h>
#include <gfx/point.h>
#include <gfx/rect.h>

namespace gfx {



  class disjoint_rects {
   public:
    disjoint_rects(const disjoint_rects &) = delete;
    disjoint_rects &operator=(const disjoint_rects &) = delete;

    disjoint_rects() {
    }
    ~disjoint_rects() {
    }

    disjoint_rects(const gfx::rect &rect) {
      m_rects.push(rect);
    }

    disjoint_rects(disjoint_rects &&) = default;
    disjoint_rects &operator=(disjoint_rects &&) = default;

    disjoint_rects clone() const {
      disjoint_rects rects;
      rects.m_rects = m_rects;
      return rects;
    }

    void move_by(int dx, int dy);
    void move_by(const gfx::point &delta) {
      move_by(delta.x(), delta.y());
    }

    void add(const gfx::rect &rect, bool allow_contained = false) {
      if (add_no_shatter(rect, allow_contained) && m_rects.size() > 1) shatter();
    }

    template <typename Container>
    void add_many(const Container &rects, bool allow_contained = false) {
      bool added = false;
      for (const auto &rect : rects) {
        if (add_no_shatter(rect, allow_contained)) added = true;
      }
      if (added && m_rects.size() > 1) shatter();
    }

    void add(const disjoint_rects &rect_set, bool allow_contained = false) {
      if (this == &rect_set) return;
      if (m_rects.is_empty()) {
        m_rects = rect_set.m_rects;
      } else {
        add_many(rect_set.rects(), allow_contained);
      }
    }

    disjoint_rects shatter(const gfx::rect &) const;
    disjoint_rects shatter(const disjoint_rects &hammer) const;

    bool contains(const gfx::rect &) const;
    bool intersects(const gfx::rect &) const;
    bool intersects(const disjoint_rects &) const;
    disjoint_rects intersected(const gfx::rect &) const;
    disjoint_rects intersected(const disjoint_rects &) const;

    template <typename Function>
    bool for_each_intersected(const gfx::rect &rect, Function f) const {
      if (is_empty() || rect.is_empty()) return true;
      for (auto &r : m_rects) {
        auto intersected_rect = r.intersect(rect);
        if (intersected_rect.is_empty()) continue;
        auto decision = f(intersected_rect);
        if (decision != true) return decision;
      }
      return true;
    }


    template <typename Function>
    bool for_each_intersected(const disjoint_rects &rects, Function f) const {
      if (is_empty() || rects.is_empty()) return true;
      if (this == &rects) {
        for (auto &r : m_rects) {
          auto decision = f(r);
          if (decision != true) return decision;
        }
      } else {
        for (auto &r : m_rects) {
          for (auto &r2 : rects.m_rects) {
            auto intersected_rect = r.intersect(r2);
            if (intersected_rect.is_empty()) continue;
            auto decision = f(intersected_rect);
            if (decision != true) return decision;
          }
        }
      }
      return true;
    }

    bool is_empty() const {
      return m_rects.is_empty();
    }
    size_t size() const {
      return m_rects.size();
    }

    void clear() {
      m_rects.clear();
    }
    void clear_with_capacity() {
      m_rects.clear_with_capacity();
    }
    const ck::vec<gfx::rect, 32> &rects() const {
      return m_rects;
    }
    ck::vec<gfx::rect, 32> take_rects() {
      return move(m_rects);
    }

   private:
    bool add_no_shatter(const gfx::rect &, bool allow_contained = false);
    void shatter();

    ck::vec<gfx::rect, 32> m_rects;
  };


}  // namespace gfx
