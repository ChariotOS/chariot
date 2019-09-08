#include <cpu.h>
#include <dev/char_dev.h>
#include <dev/driver.h>
#include <errno.h>
#include <fifo_buf.h>
#include <idt.h>
#include <mem.h>
#include <module.h>
#include <printk.h>
#include <vga.h>
#include "../majors.h"
#include "keycode.h"

#define IRQ_KEYBOARD 1
#define I8042_BUFFER 0x60
#define I8042_STATUS 0x64
#define I8042_ACK 0xFA
#define I8042_BUFFER_FULL 0x01
#define I8042_WHICH_BUFFER 0x20
#define I8042_MOUSE_BUFFER 0x20
#define I8042_KEYBOARD_BUFFER 0x00

static fifo_buf kbd_buf;

static char map[0x80] = {
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
    0x08,
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
    0x08,
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

static KeyCode unshifted_key_map[0x80] = {
    Key_Invalid,    Key_Escape,    Key_1,           Key_2,
    Key_3,          Key_4,         Key_5,           Key_6,
    Key_7,          Key_8,         Key_9,           Key_0,
    Key_Minus,      Key_Equal,     Key_Backspace,
    Key_Tab,  // 15
    Key_Q,          Key_W,         Key_E,           Key_R,
    Key_T,          Key_Y,         Key_U,           Key_I,
    Key_O,          Key_P,         Key_LeftBracket, Key_RightBracket,
    Key_Return,   // 28
    Key_Control,  // 29
    Key_A,          Key_S,         Key_D,           Key_F,
    Key_G,          Key_H,         Key_J,           Key_K,
    Key_L,          Key_Semicolon, Key_Apostrophe,  Key_Backtick,
    Key_LeftShift,  // 42
    Key_Backslash,  Key_Z,         Key_X,           Key_C,
    Key_V,          Key_B,         Key_N,           Key_M,
    Key_Comma,      Key_Period,    Key_Slash,
    Key_RightShift,  // 54
    Key_Invalid,
    Key_Alt,      // 56
    Key_Space,    // 57
    Key_Invalid,  // 58
    Key_F1,         Key_F2,        Key_F3,          Key_F4,
    Key_F5,         Key_F6,        Key_F7,          Key_F8,
    Key_F9,         Key_F10,       Key_Invalid,
    Key_Invalid,  // 70
    Key_Home,       Key_Up,        Key_PageUp,      Key_Invalid,
    Key_Left,       Key_Invalid,
    Key_Right,  // 77
    Key_Invalid,    Key_End,
    Key_Down,  // 80
    Key_PageDown,   Key_Invalid,
    Key_Delete,  // 83
    Key_Invalid,    Key_Invalid,   Key_Backslash,   Key_F11,
    Key_F12,        Key_Invalid,   Key_Invalid,     Key_Logo,
};

static KeyCode shifted_key_map[0x100] = {
    Key_Invalid,
    Key_Escape,
    Key_ExclamationPoint,
    Key_AtSign,
    Key_Hashtag,
    Key_Dollar,
    Key_Percent,
    Key_Circumflex,
    Key_Ampersand,
    Key_Asterisk,
    Key_LeftParen,
    Key_RightParen,
    Key_Underscore,
    Key_Plus,
    Key_Backspace,
    Key_Tab,
    Key_Q,
    Key_W,
    Key_E,
    Key_R,
    Key_T,
    Key_Y,
    Key_U,
    Key_I,
    Key_O,
    Key_P,
    Key_LeftBrace,
    Key_RightBrace,
    Key_Return,
    Key_Control,
    Key_A,
    Key_S,
    Key_D,
    Key_F,
    Key_G,
    Key_H,
    Key_J,
    Key_K,
    Key_L,
    Key_Colon,
    Key_DoubleQuote,
    Key_Tilde,
    Key_LeftShift,  // 42
    Key_Pipe,
    Key_Z,
    Key_X,
    Key_C,
    Key_V,
    Key_B,
    Key_N,
    Key_M,
    Key_LessThan,
    Key_GreaterThan,
    Key_QuestionMark,
    Key_RightShift,  // 54
    Key_Invalid,
    Key_Alt,
    Key_Space,    // 57
    Key_Invalid,  // 58
    Key_F1,
    Key_F2,
    Key_F3,
    Key_F4,
    Key_F5,
    Key_F6,
    Key_F7,
    Key_F8,
    Key_F9,
    Key_F10,
    Key_Invalid,
    Key_Invalid,  // 70
    Key_Home,
    Key_Up,
    Key_PageUp,
    Key_Invalid,
    Key_Left,
    Key_Invalid,
    Key_Right,  // 77
    Key_Invalid,
    Key_End,
    Key_Down,  // 80
    Key_PageDown,
    Key_Invalid,
    Key_Delete,  // 83
    Key_Invalid,
    Key_Invalid,
    Key_Pipe,
    Key_F11,
    Key_F12,
    Key_Invalid,
    Key_Invalid,
    Key_Logo,
};
bool shifted = false;

static void kbd_handler(int i, struct trapframe *tf) {
  cpu::scoped_cli scli;

  for (;;) {
    u8 status = inb(I8042_STATUS);
    if (!(((status & I8042_WHICH_BUFFER) == I8042_KEYBOARD_BUFFER) &&
          (status & I8042_BUFFER_FULL)))
      return;
    u8 raw = inb(I8042_BUFFER);
    u8 ch = raw & 0x7f;
    bool pressed = !(raw & 0x80);


#ifdef KEYBOARD_DEBUG
    dbgprintf("Keyboard::handle_irq: %b %s\n", ch, pressed ? "down" : "up");
#endif
    switch (ch) {
      case 0x38:
        // printk("alt pressed\n");
        break;
      case 0x1d:
        // printk("ctrl pressed\n");
        break;
      case 0x5b:
        // printk("super pressed\n");
        break;
      case 0x2a:
      case 0x36:
        shifted = pressed;
        // printk("shift pressed\n");
        break;
    }
    switch (ch) {
      case I8042_ACK:
        break;
      default:
        kbd_buf.write(&raw, 1);

        // if (pressed) printk("%c", (shifted ? shift_map : map)[ch]);
        // printk("default %d %d '%c'\n", pressed, shifted, (shifted ? shift_map
        // : map)[ch]);
        /*
        if (m_modifiers & Mod_Alt) {
            switch (map[ch]) {
            case '1':
            case '2':
            case '3':
            case '4':
                VirtualConsole::switch_to(map[ch] - '0' - 1);
                break;
            default:
                key_state_changed(ch, pressed);
                break;
            }
        } else {
            key_state_changed(ch, pressed);
        }
        */
    }
  }
}

class kbd_dev : public dev::char_dev {
 public:
  kbd_dev(ref<dev::driver> dr) : dev::char_dev(dr) {}

  virtual int read(u64 offset, u32 len, void *dst) override {
    // TODO: do I need to disable interrupts here (?)
    cpu::scoped_cli scli;
    return kbd_buf.read((u8 *)dst, len);
  }
  virtual int write(u64 offset, u32 len, const void *) override {
    // do nothing
    return 0;
  }
  virtual ssize_t size(void) override { return mem_size(); }
};

class kbd_driver : public dev::driver {
 public:
  kbd_driver() {
    // register the memory device on minor 0
    m_dev = make_ref<kbd_dev>(ref<kbd_driver>(this));
    dev::register_name("kbd", MAJOR_KEYBOARD, 0);
  }

  virtual ~kbd_driver(){};

  ref<dev::device> open(major_t maj, minor_t min, int &err) {
    // only work for min 0
    if (min == 0) return m_dev;

    return {};
  }

  virtual const char *name(void) const { return "kbd"; }

 private:
  ref<kbd_dev> m_dev;
};

static void dev_init(void) {
  interrupt_register(32 + IRQ_KEYBOARD, kbd_handler);

  // clear out the buffer...
  while (inb(I8042_STATUS) & I8042_BUFFER_FULL) inb(I8042_BUFFER);

  dev::register_driver(MAJOR_KEYBOARD, make_ref<kbd_driver>());
}

module_init("kbd", dev_init);
