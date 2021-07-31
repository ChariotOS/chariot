#pragma once

#include <ck/ptr.h>
#include <ck/vec.h>
#include <stdint.h>
#include <math.h>


namespace term {

  enum class State {
    WAIT_ESC,
    EXPECT_BRACKET,
    READ_ATTRIBUTE,
  };

  struct param {
    uint32_t value;
    bool empty;
  };


  struct cursor {
    int x;
    int y;
    bool visible;
  };


  enum color {
    BLACK,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    GREY,
    BRIGHT_BLACK,
    BRIGHT_RED,
    BRIGHT_GREEN,
    BRIGHT_YELLOW,
    BRIGHT_BLUE,
    BRIGHT_MAGENTA,
    BRIGHT_CYAN,
    BRIGHT_GREY,

    FOREGROUND,
    BACKGROUND,

    _COLOR_COUNT
  };

  struct attributes {
    term::color fg = term::FOREGROUND;
    term::color bg = term::BACKGROUND;
    bool bold = false;
    bool invert = false;
    bool underline = false;

   public:
    attributes bolded() const {
      attributes attr = *this;
      attr.bold = true;
      return attr;
    }

    attributes inverted() const {
      attributes attr = *this;
      attr.invert = true;
      return attr;
    }

    attributes underlined() const {
      attributes attr = *this;
      attr.underline = true;
      return attr;
    }

    attributes reset() const {
      attributes attr = *this;
      attr.bold = false;
      attr.invert = false;
      attr.underline = false;
      return attr;
    }

    attributes with_forground(term::color color) const {
      attributes attr = *this;
      attr.fg = color;
      return attr;
    }

    attributes with_background(term::color color) const {
      attributes attr = *this;
      attr.bg = color;
      return attr;
    }

    bool operator==(const attributes &other) const {
      return fg == other.fg && bg == other.bg && bold == other.bold && invert == other.invert &&
             underline == other.underline;
    }
    bool operator!=(const attributes &other) const {
      return !(fg == other.fg && bg == other.bg && bold == other.bold && invert == other.invert &&
               underline == other.underline);
    }
  };




  struct cell {
    uint32_t cp = U' ';
    attributes attr;
    bool dirty = false;
  };



  class buffer {
   public:
    buffer(int width, int height) : m_width(width), m_height(height) {
      m_cells.resize(m_width * m_height);
    }

    int width() const { return m_width; }
    int height() const { return m_height; }

    const term::cell at(int x, int y) const {
      if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        return m_cells[y * m_width + x];
      }
      return {U' ', {}, true};
    }

    void set(int x, int y, term::cell cell) {
      if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        auto old_cell = m_cells[y * m_width + x];

        if (old_cell.cp != cell.cp || old_cell.attr != cell.attr) {
          m_cells[y * m_width + x] = cell;
          m_cells[y * m_width + x].dirty = true;
        }
      }
    }


    void undirty(int x, int y) {
      if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        m_cells[y * m_width + x].dirty = false;
      }
    }

    void clear(int fromx, int fromy, int tox, int toy, term::attributes attributes) {
      for (int i = fromx + fromy * m_width; i < tox + toy * m_width; i++) {
        set(i % m_width, i / m_width, (term::cell){U' ', attributes, true});
      }
    }


    void clear_all(term::attributes attributes) { clear(0, 0, m_width, m_height, attributes); }

    void clear_line(int line, term::attributes attributes) {
      if (line >= 0 && line < m_height) {
        for (int i = 0; i < m_width; i++) {
          set(i, line, (term::cell){U' ', attributes, true});
        }
      }
    }


    void resize(int width, int height) {
      ck::vec<term::cell> new_buffer;
      new_buffer.resize(width * height);

      for (int i = 0; i < width * height; i++) {
        new_buffer[i] = {U' ', {}, true};
      }

      for (int x = 0; x < MIN(width, m_width); x++) {
        for (int y = 0; y < MIN(height, m_height); y++) {
          new_buffer[y * width + x] = at(x, y);
        }
      }

      m_cells = new_buffer;

      m_width = width;
      m_height = height;
    }



    void scroll(int how_many_line, term::attributes attributes) {
      if (how_many_line < 0) {
        for (int line = 0; line < how_many_line; line++) {
          for (int i = (width() * height()) - 1; i >= height(); i++) {
            int x = i % width();
            int y = i / width();

            set(x, y, at(x, y - 1));
          }

          clear_line(0, attributes);
        }
      } else if (how_many_line > 0) {
        for (int line = 0; line < how_many_line; line++) {
          for (int i = 0; i < width() * (height() - 1); i++) {
            int x = i % width();
            int y = i / width();

            set(x, y, at(x, y + 1));
          }

          clear_line(height() - 1, attributes);
        }
      }
    }


   private:
    int m_width;
    int m_height;
    ck::vec<term::cell> m_cells;
  };


  class surface {
   public:
    int width() { return m_width; }

    int height() { return m_height; }

    int sollback() { return m_scrollback; }

    surface(int width, int height) : m_buffer{width, 100} {
      m_width = width;
      m_height = height;
    }

    const term::cell at(int x, int y) const { return m_buffer.at(x, convert_y(y)); }

    void set(int x, int y, term::cell cell) { m_buffer.set(x, convert_y(y), cell); }

    void undirty(int x, int y) { m_buffer.undirty(x, convert_y(y)); }

    void clear(int fromx, int fromy, int tox, int toy, term::attributes attributes) {
      m_buffer.clear(fromx, convert_y(fromy), tox, convert_y(toy), attributes);
    }

    void clear_all(term::attributes attributes) { clear(0, 0, m_width, m_height, attributes); }

    void clear_line(int line, term::attributes attributes) {
      m_buffer.clear_line(convert_y(line), attributes);
    }

    void resize(int width, int height) {
      m_buffer.resize(MAX(m_buffer.width(), width), MAX(MAX(height, m_height), 1000));
      m_width = width;
      m_height = height;
    }

    void scroll(int how_many_line, term::attributes attributes) {
      m_buffer.scroll(how_many_line, attributes);
      m_scrollback += how_many_line;
    }

   private:
    term::buffer m_buffer;

    int m_height;
    int m_width;

    int m_scrollback = 0;

    int convert_y(int y) const { return m_buffer.height() - m_height + y; }
  };




  class terminal : public ck::refcounted<term::terminal> {
   public:
    terminal(int width, int height);

    void resize(int width, int height);
    void cursor_move(int offx, int offy);
    void cursor_set(int x, int y);
    void new_line();
    void backspace();
    void append(uint32_t codepoint);
    void do_ansi(uint32_t codepoint);
    void write(uint32_t codepoint);
    // void write(char c);
    void write(const char *buffer, size_t size);


    int width() { return m_surface.width(); }
    int height() { return m_surface.height(); }

    auto &surface() { return m_surface; }
    auto &cursor() { return m_cursor; }

   private:
    term::State m_state = term::State::WAIT_ESC;

    term::cursor m_cursor = {0, 0, true};
    term::cursor m_saved_cursor = {0, 0, true};

    term::attributes m_attributes;

    static constexpr int MAX_PARAMETERS = 8;
    int m_parameters_top;
    term::param m_parameters[MAX_PARAMETERS];

    term::surface m_surface;
  };
}  // namespace term
