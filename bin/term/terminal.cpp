#include "terminal.h"
#include <ctype.h>



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
  if (codepoint == U'\n') {
    new_line();
  } else if (codepoint == U'\r') {
    cursor_move(-m_cursor.x, 0);
  } else if (codepoint == U'\t') {
    cursor_move(8 - (m_cursor.x % 8), 0);
  } else if (codepoint == U'\b') {
    backspace();
  } else {
    m_surface.set(m_cursor.x, m_cursor.y, {codepoint, m_attributes, true});
    cursor_move(1, 0);
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
      if (codepoint == U'\e') {
        for (auto &p : m_parameters) {
          p.empty = true;
          p.value = 0;
        }

        m_parameters_top = 0;

        m_state = State::EXPECT_BRACKET;
      } else {
        m_state = State::WAIT_ESC;
        append(codepoint);
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
