#include "terminal.h"
#include <ctype.h>




////////////////////////////////////////////////////////////////////////////////////////




term::buffer::buffer(int width, int height) : m_width(width), m_height(height) {
  m_rows.resize(m_height);
  for (int i = 0; i < m_height; i++) {
    m_rows[i] = {};
  }
  // m_cells.resize(m_width * m_height);
}


term::buffer::~buffer(void) {}

const term::cell term::buffer::at(int x, int y) const {
  if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
    if (m_rows[y] != NULL) {
      return m_rows[y][x];
    }
    // return m_cells[y * m_width + x];
  }
  return {U' ', {}, true};
}

void term::buffer::set(int x, int y, term::cell cell) {
  if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
    if (m_rows[y] == NULL) {
      // allocate the row
      m_rows[y] = new term::cell[m_width];
    }

    term::cell old_cell = m_rows[y][x];  // m_cells[y * m_width + x];

    if (old_cell.cp != cell.cp || old_cell.attr != cell.attr) {
      m_rows[y][x] = cell;
      m_rows[y][x].dirty = true;
    }
  }
}


void term::buffer::undirty(int x, int y) {
  if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
    if (m_rows[y] == NULL) {
      m_rows[y] = allocate_row();
    }
    m_rows[y][x].dirty = false;
  }
}

void term::buffer::clear(int fromx, int fromy, int tox, int toy, term::attributes attributes) {
  for (int i = fromx + fromy * m_width; i < tox + toy * m_width; i++) {
    set(i % m_width, i / m_width, (term::cell){U' ', attributes, true});
  }
}


void term::buffer::clear_all(term::attributes attributes) {
  clear(0, 0, m_width, m_height, attributes);
}

void term::buffer::clear_line(int line, term::attributes attributes) {
  if (line >= 0 && line < m_height) {
    for (int i = 0; i < m_width; i++) {
      set(i, line, (term::cell){U' ', attributes, true});
    }
  }
}

void term::buffer::resize(int width, int height) {
  // make a new buffer to replace the old one
  decltype(m_rows) new_buffer;
  new_buffer.resize(width * height);

  if (width != m_width) {
    while (!m_preallocated_rows.is_empty()) {
      delete allocate_row();
    }
  }

  // TODO: use realloc :)
  for (int row = 0; row < MIN(height, m_height); row++) {
    if (m_rows[row] != NULL) {
      new_buffer[row] = allocate_row();
      for (int col = 0; col < MIN(width, m_width); col++) {
        new_buffer[row][col] = m_rows[row][col];
      }
      // delete the old row backing storage
      if (width != m_width) {
        delete m_rows[row];
      } else {
        release_row(m_rows[row]);
      }
      m_rows[row] = NULL;
    }
  }

  m_rows = move(new_buffer);

  m_width = width;
  m_height = height;
}



void term::buffer::scroll(int how_many_line, term::attributes attributes) {
  if (how_many_line < 0) {
    // move cells down
    // TODO: heavily optimize this lol
    for (int line = 0; line < how_many_line; line++) {
      for (int i = (width() * height()) - 1; i >= height(); i++) {
        int x = i % width();
        int y = i / width();
        // move the cell down a line
        set(x, y, at(x, y - 1));
      }

      clear_line(0, attributes);
    }
  } else if (how_many_line > 0) {
    // move cells up

    // delete the top lines
    for (int line = 0; line < how_many_line; line++) {
      release_row(m_rows[line]);
      m_rows[line] = NULL;
    }

    for (int row = how_many_line; row < m_height - how_many_line; row++) {
      m_rows[row] = m_rows[row + how_many_line];
      m_rows[row + how_many_line] = NULL;
    }
  }
}


term::cell *term::buffer::allocate_row(void) {
  return new term::cell[m_width];

  printf("%d\n", m_preallocated_rows.size());
  if (m_preallocated_rows.is_empty()) {
    return new term::cell[m_width];
  }

  int last = m_preallocated_rows.size() - 1;
  auto *row = m_preallocated_rows[last];
  m_preallocated_rows.remove(last);

  return row;
}

void term::buffer::release_row(term::cell *row) {
  delete row;
  return;
  m_preallocated_rows.push(row);
}

////////////////////////////////////////////////////////////////////////////////////////


term::terminal::terminal(int width, int height) : m_surface(width, height) {
  for (auto &p : m_parameters) {
    p.empty = true;
    p.value = 0;
  }
}

void term::terminal::resize(int width, int height) {
  m_surface.resize(width, height);

  m_cursor.x = CLAMP(m_cursor.x, 0, width - 1);
  m_cursor.y = CLAMP(m_cursor.y, 0, height - 1);
}


void term::terminal::cursor_move(int offx, int offy) {
  if (m_cursor.x + offx < 0) {
    int old_cursor_x = m_cursor.x;
    m_cursor.x = width() + ((old_cursor_x + offx) % width());
    cursor_move(0, offy + ((old_cursor_x + offx) / width() - 1));
  } else if (m_cursor.x + offx >= width()) {
    int old_cursor_x = m_cursor.x;
    m_cursor.x = (old_cursor_x + offx) % width();
    cursor_move(0, offy + (old_cursor_x + offx) / width());
  } else if (m_cursor.y + offy < 0) {
    m_surface.scroll(m_cursor.y + offy, m_attributes);
    m_cursor.y = 0;

    cursor_move(offx, 0);
  } else if (m_cursor.y + offy >= height()) {
    m_surface.scroll((m_cursor.y + offy) - (height() - 1), m_attributes);
    m_cursor.y = height() - 1;

    cursor_move(offx, 0);
  } else {
    m_cursor.x += offx;
    m_cursor.y += offy;

    assert(m_cursor.x >= 0 && m_cursor.x < width());
    assert(m_cursor.y >= 0 && m_cursor.y < height());
  }
}



void term::terminal::cursor_set(int x, int y) {
  m_cursor.x = CLAMP(x, 0, width());
  m_cursor.y = CLAMP(y, 0, height());
}

void term::terminal::new_line() { cursor_move(-m_cursor.x, 1); }

void term::terminal::backspace() { cursor_move(-1, 0); }


void term::terminal::append(uint32_t codepoint) {
  switch (codepoint) {
    case U'\n':
      new_line();
      break;

    case U'\r':
      cursor_move(-m_cursor.x, 0);
      break;

    case U'\t':
      cursor_move(8 - (m_cursor.x % 8), 0);
      break;

    case U'\b':
      backspace();
      break;

    default:
      m_surface.set(m_cursor.x, m_cursor.y, {codepoint, m_attributes, true});
      cursor_move(1, 0);
      break;
  }
}



void term::terminal::do_ansi(uint32_t codepoint) {
  switch (codepoint) {
    case U'A':
      if (m_parameters[0].empty) {
        cursor_move(0, -1);
      } else {
        cursor_move(0, -m_parameters[0].value);
      }
      break;

    case U'B':
      if (m_parameters[0].empty) {
        cursor_move(0, 1);
      } else {
        cursor_move(0, m_parameters[0].value);
      }
      break;

    case U'C':
      if (m_parameters[0].empty) {
        cursor_move(1, 0);
      } else {
        cursor_move(m_parameters[0].value, 0);
      }
      break;

    case U'D':
      if (m_parameters[0].empty) {
        cursor_move(-1, 0);
      } else {
        cursor_move(-m_parameters[0].value, 0);
      }
      break;

    case U'E':
      if (m_parameters[0].empty) {
        cursor_move(-m_cursor.x, 1);
      } else {
        cursor_move(-m_cursor.x, m_parameters[0].value);
      }
      break;

    case U'F':
      if (m_parameters[0].empty) {
        cursor_move(-m_cursor.x, -1);
      } else {
        cursor_move(-m_cursor.x, -m_parameters[0].value);
      }
      break;

    case U'G':
      if (m_parameters[0].empty) {
        cursor_move(0, m_cursor.y);
      } else {
        cursor_move(m_parameters[0].value - 1, m_cursor.y);
      }
      break;

    case U'f':
    case U'H': {
      int row;
      int column;

      if (m_parameters[0].empty) {
        row = 0;
      } else {
        row = m_parameters[0].value - 1;
      }

      if (m_parameters[1].empty) {
        column = 0;
      } else {
        column = m_parameters[1].value - 1;
      }

      cursor_set(column, row);
    } break;

    case U'J':
      if (m_parameters[0].value == 0) {
        m_surface.clear(m_cursor.x, m_cursor.y, width(), height(), m_attributes);
      } else if (m_parameters[0].value == 1) {
        m_surface.clear(0, 0, m_cursor.x, m_cursor.y, m_attributes);
      } else if (m_parameters[0].value == 2) {
        m_surface.clear(0, 0, width(), height(), m_attributes);
      }
      break;

    case U'K':
      if (m_parameters[0].value == 0) {
        m_surface.clear(m_cursor.x, m_cursor.y, width(), m_cursor.y, m_attributes);
      } else if (m_parameters[0].value == 1) {
        m_surface.clear(0, m_cursor.y, m_cursor.x, m_cursor.y, m_attributes);
      } else if (m_parameters[0].value == 2) {
        m_surface.clear(0, m_cursor.y, width(), m_cursor.y, m_attributes);
      }
      break;

    case U'S':
      if (m_parameters[0].empty) {
        m_surface.scroll(-1, m_attributes);
      } else {
        m_surface.scroll(-m_parameters[0].value, m_attributes);
      }
      break;

    case U'T':
      if (m_parameters[0].empty) {
        m_surface.scroll(1, m_attributes);
      } else {
        m_surface.scroll(m_parameters[0].value, m_attributes);
      }
      break;

    case U'm':
      for (int i = 0; i <= m_parameters_top; i++) {
        if (m_parameters[i].empty || m_parameters[i].value == 0) {
          m_attributes = {};
        } else {
          int attr = m_parameters[i].value;

          if (attr == 1) {
            m_attributes = m_attributes.bolded();
          } else if (attr == 3) {
            m_attributes = m_attributes.inverted();
          } else if (attr == 4) {
            m_attributes = m_attributes.underlined();
          } else if (attr >= 30 && attr <= 37) {
            m_attributes = m_attributes.with_forground((term::color)(attr - 30));
          } else if (attr >= 90 && attr <= 97) {
            m_attributes = m_attributes.with_forground((term::color)(attr - 90 + 8));
          } else if (attr >= 40 && attr <= 47) {
            m_attributes = m_attributes.with_background((term::color)(attr - 40));
          } else if (attr >= 100 && attr <= 107) {
            m_attributes = m_attributes.with_background((term::color)(attr - 100 + 8));
          }
        }
      }
      break;

    case U's':
      m_saved_cursor = m_cursor;
      break;

    case U'u':
      m_cursor = m_saved_cursor;
      break;

    default:
      break;
  }
}



void term::terminal::write(uint32_t codepoint) {
  switch (m_state) {
    case State::WAIT_ESC:
      // common case
      if (codepoint != U'\e') {
        m_state = State::WAIT_ESC;
        append(codepoint);
      } else {
        for (auto &p : m_parameters) {
          p.empty = true;
          p.value = 0;
        }

        m_parameters_top = 0;

        m_state = State::EXPECT_BRACKET;
      }
      break;

    case State::EXPECT_BRACKET:
      if (codepoint == U'[') {
        m_state = State::READ_ATTRIBUTE;
      } else if (codepoint == U'c') {
        m_attributes = {};
        m_state = State::WAIT_ESC;

        cursor_set(0, 0);
        m_surface.clear_all(m_attributes);
      } else {
        m_state = State::WAIT_ESC;
        append(codepoint);
      }
      break;

    case State::READ_ATTRIBUTE:
      if (isdigit(codepoint)) {
        m_parameters[m_parameters_top].empty = false;
        m_parameters[m_parameters_top].value *= 10;
        m_parameters[m_parameters_top].value += codepoint - U'0';
      } else {
        if (codepoint == U';') {
          m_parameters_top++;
        } else {
          m_state = State::WAIT_ESC;

          do_ansi(codepoint);
        }
      }
      break;

    default:
      panic("oh no");
  }
}

void term::terminal::write(const char *buffer, size_t size) {
  for (size_t i = 0; i < size; i++) {
    write(buffer[i]);
  }
}
