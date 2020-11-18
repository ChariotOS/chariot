#include <gfx/disjoint_rects.h>


namespace gfx {

  bool disjoint_rects::add_no_shatter(const gfx::rect& new_rect, bool allow_contained) {
    if (new_rect.is_empty()) return false;
    for (auto& rect : m_rects) {
    	if (!allow_contained && rect.contains(new_rect)) return false;
    }

    m_rects.push(new_rect);
    return true;
  }

  void disjoint_rects::shatter() {
    ck::vec<gfx::rect, 32> output;
    output.ensure_capacity(m_rects.size());
    bool pass_had_intersections = false;
    do {
      pass_had_intersections = false;
      output.clear_with_capacity();
      for (size_t i = 0; i < m_rects.size(); ++i) {
        auto& r1 = m_rects[i];
        for (size_t j = 0; j < m_rects.size(); ++j) {
          if (i == j) continue;
          auto& r2 = m_rects[j];
          if (!r1.intersects(r2)) continue;
          pass_had_intersections = true;
          auto pieces = r1.shatter(r2);
          for (auto& piece : pieces) output.push(piece);
          m_rects.remove(i);
          for (; i < m_rects.size(); ++i) output.push(m_rects[i]);
          goto next_pass;
        }
        output.push(r1);
      }
    next_pass:
      swap(output, m_rects);
    } while (pass_had_intersections);
  }

  void disjoint_rects::move_by(int dx, int dy) {
    for (auto& r : m_rects) {
      r.x += dx;
      r.y += dy;
    }
  }

  bool disjoint_rects::contains(const gfx::rect& rect) const {
    if (is_empty() || rect.is_empty()) return false;

    // TODO: This could use some optimization
    disjoint_rects remainder(rect);
    for (auto& r : m_rects) {
      auto shards = remainder.shatter(r);
      if (shards.is_empty()) return true;
      remainder = move(shards);
    }
    return false;
  }

  bool disjoint_rects::intersects(const gfx::rect& rect) const {
    for (auto& r : m_rects) {
      if (r.intersects(rect)) return true;
    }
    return false;
  }

  bool disjoint_rects::intersects(const disjoint_rects& rects) const {
    if (this == &rects) return true;

    for (auto& r : m_rects) {
      for (auto& r2 : rects.m_rects) {
        if (r.intersects(r2)) return true;
      }
    }
    return false;
  }

  disjoint_rects disjoint_rects::intersected(const gfx::rect& rect) const {
    disjoint_rects intersected_rects;
    intersected_rects.m_rects.ensure_capacity(m_rects.capacity());
    for (auto& r : m_rects) {
      auto intersected_rect = r.intersect(rect);
      if (!intersected_rect.is_empty()) intersected_rects.m_rects.push(intersected_rect);
    }
    // Since there should be no overlaps, we don't need to call shatter()
    return intersected_rects;
  }

  disjoint_rects disjoint_rects::intersected(const disjoint_rects& rects) const {
    if (&rects == this) return clone();
    if (is_empty() || rects.is_empty()) return {};

    disjoint_rects intersected_rects;
    intersected_rects.m_rects.ensure_capacity(m_rects.capacity());
    for (auto& r : m_rects) {
      for (auto& r2 : rects.m_rects) {
        auto intersected_rect = r.intersect(r2);
        if (!intersected_rect.is_empty()) intersected_rects.m_rects.push(intersected_rect);
      }
    }
    // Since there should be no overlaps, we don't need to call shatter()
    return intersected_rects;
  }

  disjoint_rects disjoint_rects::shatter(const gfx::rect& hammer) const {
    if (hammer.is_empty()) return clone();

    disjoint_rects shards;
    for (auto& rect : m_rects) {
      for (auto& shard : rect.shatter(hammer)) shards.add_no_shatter(shard);
    }
    // Since there should be no overlaps, we don't need to call shatter()
    return shards;
  }

  disjoint_rects disjoint_rects::shatter(const disjoint_rects& hammer) const {
    if (this == &hammer) return {};
    if (hammer.is_empty() || !intersects(hammer)) return clone();

    // TODO: This could use some optimization
    disjoint_rects shards = shatter(hammer.m_rects[0]);
    auto rects_count = hammer.m_rects.size();
    for (size_t i = 1; i < rects_count && !shards.is_empty(); i++) {
      bool intersects = false;
      for (auto& r : shards.m_rects) {
        if (hammer.m_rects[i].intersects(r)) {
          intersects = true;
          break;
        }
      }
      if (intersects) {
        auto shattered = shards.shatter(hammer.m_rects[i]);
        shards = move(shattered);
      }
    }
    // Since there should be no overlaps, we don't need to call shatter()
    return shards;
  }

}  // namespace gfx
