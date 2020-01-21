#include <asm.h>
#include <module.h>
#include <pcspeaker.h>
#include <pit.h>
#include <types.h>

#define TICKS_PER_SECOND 1000
/* Timer related ports */
#define TIMER0_CTL 0x40
#define TIMER1_CTL 0x41
#define TIMER2_CTL 0x42
#define PIT_CTL 0x43
#define TIMER0_SELECT 0x00
#define TIMER1_SELECT 0x40
#define TIMER2_SELECT 0x80

#define MODE_COUNTDOWN 0x00
#define MODE_ONESHOT 0x02
#define MODE_RATE 0x04
#define MODE_SQUARE_WAVE 0x06
#define WRITE_WORD 0x30
#define BASE_FREQUENCY 1193182
#define LSB(x) ((x)&0xFF)
#define MSB(x) (((x) >> 8) & 0xFF)

void pcspeaker::set(int tone) {
  if (tone == 0) return;
  outb(PIT_CTL, TIMER2_SELECT | WRITE_WORD | MODE_SQUARE_WAVE);
  u16 timer_reload = BASE_FREQUENCY / tone;

  outb(TIMER2_CTL, LSB(timer_reload));
  outb(TIMER2_CTL, MSB(timer_reload));
  outb(0x61, inb(0x61) | 3);
}

void pcspeaker::clear(void) { outb(0x61, inb(0x61) & ~3); }

void pcspeaker_init(void) {
  //
}

// module_init("pcspeaker", pcspeaker_init);
