#pragma once


namespace gui {

  struct rect {
    // simple!
    int x, y, w, h;

    void draw(int color);

		inline rect() {
			x = y = w = h = 0;
		}

    inline rect(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}
		inline rect(const rect &o) {
			rect(o.x, o.y, o.w, o.h);
		}

		inline rect &operator=(const rect &o) {
			x = o.x;
			y = o.y;
			w = o.w;
			h = o.h;
			return *this;
		}

    inline int left() const { return x; }
    inline int right() const { return x + w; }
    inline int top() const { return y; }
    inline int bottom() const { return y + h; }

    struct rect intersect(const struct rect &other) const;

    inline bool intersects(const rect &other) const {
      return left() <= other.right() && other.left() <= right() &&
             top() <= other.bottom() && other.top() <= bottom();
    }
  };

};  // namespace gui
