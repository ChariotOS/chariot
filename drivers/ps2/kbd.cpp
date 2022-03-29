#include <arch.h>
#include <console.h>
#include <dev/char_dev.h>
#include <dev/driver.h>
#include <errno.h>
#include <fifo_buf.h>
#include <keycode.h>
#include <mem.h>
#include <module.h>
#include <printf.h>
#include <ck/single_list.h>
#include <syscall.h>
#include <vga.h>

#include "../majors.h"

#define IRQ_KEYBOARD 1
#define I8042_BUFFER 0x60
#define I8042_STATUS 0x64
#define I8042_ACK 0xFA
#define I8042_BUFFER_FULL 0x01
#define I8042_WHICH_BUFFER 0x20
#define I8042_MOUSE_BUFFER 0x20
#define I8042_KEYBOARD_BUFFER 0x00

static fifo_buf kbd_buf;

static long owners = 0;

static char keymap[0x80] = {
    0,
    '\033',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0',
    '-',
    '=',
    CONS_DEL,
    '\t',
    'q',
    'w',
    'e',
    'r',
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    '\n',
    0,
    'a',
    's',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';',
    '\'',
    '`',
    0,
    '\\',
    'z',
    'x',
    'c',
    'v',
    'b',
    'n',
    'm',
    ',',
    '.',
    '/',
    0,
    0,
    0,
    ' ',
    0,
    0,
    // 60
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    // 70
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    // 80
    0,
    0,
    0,
    0,
    0,
    0,
    '\\',
    0,
    0,
    0,
};

static char shift_map[0x80] = {
    0,
    '\033',
    '!',
    '@',
    '#',
    '$',
    '%',
    '^',
    '&',
    '*',
    '(',
    ')',
    '_',
    '+',
    CONS_DEL,
    '\t',
    'Q',
    'W',
    'E',
    'R',
    'T',
    'Y',
    'U',
    'I',
    'O',
    'P',
    '{',
    '}',
    '\n',
    0,
    'A',
    'S',
    'D',
    'F',
    'G',
    'H',
    'J',
    'K',
    'L',
    ':',
    '"',
    '~',
    0,
    '|',
    'Z',
    'X',
    'C',
    'V',
    'B',
    'N',
    'M',
    '<',
    '>',
    '?',
    0,
    0,
    0,
    ' ',
    0,
    0,
    // 60
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    // 70
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    // 80
    0,
    0,
    0,
    0,
    0,
    0,
    '|',
    0,
    0,
    0,
};


static keycode unshifted_key_map[0x80] = {
    key_invalid,
    key_escape,
    key_1,
    key_2,
    key_3,
    key_4,
    key_5,
    key_6,
    key_7,
    key_8,
    key_9,
    key_0,
    key_minus,
    key_equal,
    key_backspace,
    key_tab,  // 15
    key_q,
    key_w,
    key_e,
    key_r,
    key_t,
    key_y,
    key_u,
    key_i,
    key_o,
    key_p,
    key_leftbracket,
    key_rightbracket,
    key_return,   // 28
    key_control,  // 29
    key_a,
    key_s,
    key_d,
    key_f,
    key_g,
    key_h,
    key_j,
    key_k,
    key_l,
    key_semicolon,
    key_apostrophe,
    key_backtick,
    key_leftshift,  // 42
    key_backslash,
    key_z,
    key_x,
    key_c,
    key_v,
    key_b,
    key_n,
    key_m,
    key_comma,
    key_period,
    key_slash,
    key_rightshift,  // 54
    key_invalid,
    key_alt,      // 56
    key_space,    // 57
    key_invalid,  // 58
    key_f1,
    key_f2,
    key_f3,
    key_f4,
    key_f5,
    key_f6,
    key_f7,
    key_f8,
    key_f9,
    key_f10,
    key_invalid,
    key_invalid,  // 70
    key_home,
    key_up,
    key_pageup,
    key_invalid,
    key_left,
    key_invalid,
    key_right,  // 77
    key_invalid,
    key_end,
    key_down,  // 80
    key_pagedown,
    key_invalid,
    key_delete,  // 83
    key_invalid,
    key_invalid,
    key_backslash,
    key_f11,
    key_f12,
    key_invalid,
    key_invalid,
    key_logo,
};

static keycode shifted_key_map[0x100] = {
    key_invalid,
    key_escape,
    key_exclamationpoint,
    key_atsign,
    key_hashtag,
    key_dollar,
    key_percent,
    key_circumflex,
    key_ampersand,
    key_asterisk,
    key_leftparen,
    key_rightparen,
    key_underscore,
    key_plus,
    key_backspace,
    key_tab,
    key_q,
    key_w,
    key_e,
    key_r,
    key_t,
    key_y,
    key_u,
    key_i,
    key_o,
    key_p,
    key_leftbrace,
    key_rightbrace,
    key_return,
    key_control,
    key_a,
    key_s,
    key_d,
    key_f,
    key_g,
    key_h,
    key_j,
    key_k,
    key_l,
    key_colon,
    key_doublequote,
    key_tilde,
    key_leftshift,  // 42
    key_pipe,
    key_z,
    key_x,
    key_c,
    key_v,
    key_b,
    key_n,
    key_m,
    key_lessthan,
    key_greaterthan,
    key_questionmark,
    key_rightshift,  // 54
    key_invalid,
    key_alt,
    key_space,    // 57
    key_invalid,  // 58
    key_f1,
    key_f2,
    key_f3,
    key_f4,
    key_f5,
    key_f6,
    key_f7,
    key_f8,
    key_f9,
    key_f10,
    key_invalid,
    key_invalid,  // 70
    key_home,
    key_up,
    key_pageup,
    key_invalid,
    key_left,
    key_invalid,
    key_right,  // 77
    key_invalid,
    key_end,
    key_down,  // 80
    key_pagedown,
    key_invalid,
    key_delete,  // 83
    key_invalid,
    key_invalid,
    key_pipe,
    key_f11,
    key_f12,
    key_invalid,
    key_invalid,
    key_logo,
};
bool shifted = false;
static u8 m_modifiers = 0;

static void update_modifier(u8 modifier, bool state) {
  if (state)
    m_modifiers |= modifier;
  else
    m_modifiers &= ~modifier;
}

static void key_state_changed(u8 raw, bool pressed) {
  keyboard_packet_t event;
  event.magic = KEY_EVENT_MAGIC;
  event.key = (m_modifiers & mod_shift) ? shifted_key_map[raw] : unshifted_key_map[raw];
  event.character = (m_modifiers & mod_shift) ? shift_map[raw] : keymap[raw];
  event.flags = m_modifiers;
  if (pressed) event.flags |= is_pressed;



  if (owners > 0) {
    kbd_buf.write(&event, sizeof(event), false /* no block */);
  } else {
    if (pressed) {
#define SER(code, replace)                              \
  case (code):                                          \
    console::feed(sizeof(replace) - 1, (char*)replace); \
    break;
      switch (event.key) {
        case key_shift:
          break;
          SER(key_left, "\x1b[1D");
          SER(key_up, "\x1b[1A");
          SER(key_right, "\x1b[1C");
          SER(key_down, "\x1b[1B");
        default:
          console::feed(1, (char*)&event.character);
          break;
      }
    }
#undef SER
  }
}

static void kbd_handler(int i, reg_t* tf, void*) {
  irq::eoi(i);

  for (;;) {
    u8 status = inb(I8042_STATUS);
    if (!(((status & I8042_WHICH_BUFFER) == I8042_KEYBOARD_BUFFER) && (status & I8042_BUFFER_FULL))) return;
    u8 raw = inb(I8042_BUFFER);
    u8 ch = raw & 0x7f;
    bool pressed = !(raw & 0x80);

#ifdef KEYBOARD_DEBUG
    dbgprintf("Keyboard::handle_irq: %b %s\n", ch, pressed ? "down" : "up");
#endif
    switch (ch) {
      case 0x38:
        update_modifier(mod_alt, pressed);
        break;
      case 0x1d:
        update_modifier(mod_ctrl, pressed);
        break;
      case 0x5b:
        update_modifier(mod_logo, pressed);
        break;
      case 0x2a:
      case 0x36:
        update_modifier(mod_shift, pressed);
        break;
    }

    switch (ch) {
      case I8042_ACK:
        break;
      default:
        key_state_changed(ch, pressed);
    }
  }
}



static ssize_t kbd_read(fs::File& fd, char* buf, size_t sz) {
  if (fd) {
    if (sz % sizeof(keyboard_packet_t) != 0) {
      return -EINVAL;
    }

    // this is a nonblocking api
    return kbd_buf.read(buf, sz, false);
  }
  return -1;
}


static int kbd_open(fs::File& fd) {
  owners++;
  return 0;
}
static void kbd_close(fs::File& fd) { owners--; }

static int kbd_poll(fs::File& fd, int events, poll_table& pt) { return kbd_buf.poll(pt); }


static void kbd_init(void) {
  irq::install(IRQ_KEYBOARD, kbd_handler, "PS2 Keyboard");

  // clear out the buffer...
  while (inb(I8042_STATUS) & I8042_BUFFER_FULL)
    inb(I8042_BUFFER);


  // TODO:
  // dev::register_driver(keyboard_driver_info);
  // dev::register_name(keyboard_driver_info, "keyboard", 0);
}

module_init("kbd", kbd_init);
