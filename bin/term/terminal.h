#pragma once

#include <ck/ptr.h>
#include <ck/vec.h>
#include <stdint.h>
#include <math.h>
#include <ck/time.h>


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
    buffer(int width, int height);
    ~buffer(void);

    int width() const { return m_width; }
    int height() const { return m_height; }

    const term::cell at(int x, int y) const;
    void set(int x, int y, term::cell cell);

    void undirty(int x, int y);
    void clear(int fromx, int fromy, int tox, int toy, term::attributes attributes);
    void clear_all(term::attributes attributes);
    void clear_line(int line, term::attributes attributes);
    void resize(int width, int height);
    void scroll(int how_many_line, term::attributes attributes);


   private:
    term::cell *allocate_row(void);
    void release_row(term::cell *);

    int m_width;
    int m_height;

    // a vector of the rows in this buffer, lazily allocated on ::set().
    // Each entry is of length m_width.
    ck::vec<term::cell *> m_rows;

    // when a row is scrolled off the top of the buffer, it goes
    // here, so future allocations can resuse it.
    ck::vec<term::cell *> m_preallocated_rows;
  };


  class surface {
   public:
    int width() { return m_width; }

    int height() { return m_height; }

    int sollback() { return m_scrollback; }

    surface(int width, int height) : m_buffer{width, 1000} {
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
