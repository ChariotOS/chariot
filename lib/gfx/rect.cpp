#include <gfx/rect.h>


#define max(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })


#define min(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
  })


ck::vec<gfx::rect, 4> gfx::rect::shatter(const gfx::rect& hammer) const {
  ck::vec<gfx::rect, 4> pieces;
  if (!intersects(hammer)) {
    pieces.push(*this);
    return pieces;
  }
  gfx::rect top_shard{x, y, w, hammer.y - y};
  gfx::rect bottom_shard{x, hammer.y + hammer.h, w, (y + h) - (hammer.y + hammer.h)};
  gfx::rect left_shard{x, max(hammer.y, y), hammer.x - x,
                       min((hammer.y + hammer.h), (y + h)) - max(hammer.y, y)};
  gfx::rect right_shard{hammer.x + hammer.w, max(hammer.y, y), right() - hammer.right(),
                        min((hammer.y + hammer.h), (y + h)) - max(hammer.y, y)};
  if (!top_shard.is_empty()) pieces.push(top_shard);
  if (!bottom_shard.is_empty()) pieces.push(bottom_shard);
  if (!left_shard.is_empty()) pieces.push(left_shard);
  if (!right_shard.is_empty()) pieces.push(right_shard);

  return pieces;
}
