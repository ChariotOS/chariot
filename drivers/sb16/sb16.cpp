#include <asm.h>
#include <cpu.h>
#include <dev/driver.h>
#include <errno.h>
#include <module.h>
#include <phys.h>
#include <printk.h>
#include <util.h>

#include "../majors.h"

#define SB16_DEFAULT_IRQ 5

enum class SampleFormat : u8 {
  Signed = 0x10,
  Stereo = 0x20,
};

constexpr uint16_t DSP_READ = 0x22A;
constexpr uint16_t DSP_WRITE = 0x22C;
constexpr uint16_t DSP_STATUS = 0x22E;
constexpr uint16_t DSP_R_ACK = 0x22F;

int m_major_version = 0;



static inline void delay(size_t microseconds) {
  for (size_t i = 0; i < microseconds; ++i)
    inb(0x80);
}

/* Write a value to the DSP write register */
static void dsp_write(uint8_t value) {
  while (inb(DSP_WRITE) & 0x80) {
    ;
  }
  outb(DSP_WRITE, value);
}

/* Reads the value of the DSP read register */
static uint8_t dsp_read() {
  while (!(inb(DSP_STATUS) & 0x80)) {
    ;
  }
  return inb(DSP_READ);
}

/* Changes the sample rate of sound output */
static void set_sample_rate(uint16_t hz) {
  dsp_write(0x41);  // output
  dsp_write((u8)(hz >> 8));
  dsp_write((u8)hz);
  dsp_write(0x42);  // input
  dsp_write((u8)(hz >> 8));
  dsp_write((u8)hz);
}


static void set_irq_register(u8 irq_number) {
  uint8_t bitmask = 0;
  switch (irq_number) {
    case 2:
      bitmask = 0;
      break;
    case 5:
      bitmask = 0b10;
      break;
    case 7:
      bitmask = 0b100;
      break;
    case 10:
      bitmask = 0b1000;
      break;
    default:
      panic("INVALID!\n");
  }
  outb(0x224, 0x80);
  outb(0x225, bitmask);
}


static uint8_t get_irq_line() {
  outb(0x224, 0x80);
  uint8_t bitmask = inb(0x225);
  switch (bitmask) {
    case 0:
      return 2;
    case 0b10:
      return 5;
    case 0b100:
      return 7;
    case 0b1000:
      return 10;
  }
  return bitmask;
}


/*
static void set_irq_line(u8 irq_number) {
        arch_disable_ints();
  InterruptDisabler disabler;
  if (irq_number == get_irq_line()) return;
  set_irq_register(irq_number);
  change_irq_number(irq_number);
        arch_enable_ints();
}
*/


static spinlock sb16_lock;
void *dma_page = NULL;
static wait_queue sb16_wq;


static void dma_start(uint32_t length) {
  const auto addr = (off_t)v2p(dma_page);
  printk(KERN_INFO "DMA START %p %d. page=%04x\n", addr, length, addr >> 16);
  const u8 channel = 5;  // 16-bit samples use DMA channel 5 (on the master DMA controller)
  const u8 mode = 0x48;

  // Disable the DMA channel
  outb(0xd4, 4 + (channel % 4));

  // Clear the byte pointer flip-flop
  outb(0xd8, 0);

  // Write the DMA mode for the transfer
  outb(0xd6, (channel % 4) | mode);

  // Write the offset of the buffer
  uint16_t offset = (addr / 2) % 65536;
  outb(0xc4, (u8)offset);
  outb(0xc4, (u8)(offset >> 8));

  // Write the transfer length
  outb(0xc6, (u8)(length - 1));
  outb(0xc6, (u8)((length - 1) >> 8));

  // Write the buffer
  outw(0x8b, addr >> 16);

  // Enable the DMA channel
  outb(0xd4, channel % 5);
}


static ssize_t sb16_write(fs::file &fd, const char *buf, size_t sz) {
  // limit to single access
  scoped_lock l(sb16_lock);
  if (dma_page == NULL) {
    dma_page = phys::kalloc(1);
  }



  const int BLOCK_SIZE = 32 * 1024;
  if (sz > PGSIZE) return -ENOSPC;
  if (sz > BLOCK_SIZE) return -ENOSPC;

  uint8_t mode = (uint8_t)SampleFormat::Signed | (uint8_t)SampleFormat::Stereo;

  const int sample_rate = 44100;
  set_sample_rate(sample_rate);
  memcpy(p2v(dma_page), buf, sz);

  // 16-bit single-cycle output.
  // FIXME: Implement auto-initialized output.
  uint8_t command = 0xb0;
  uint16_t sample_count = sz / sizeof(int16_t);
  // if stereo, double the sample are used
  if (mode & (u8)SampleFormat::Stereo) sample_count >>= 1;

  printk(KERN_INFO "SB16: writing %d bytes! %d samples\n", sz, sample_count);

  sample_count -= 1;

  dma_start(sz);


  dsp_write(command);
  dsp_write(mode);
  dsp_write(sample_count & 0xFF);
  dsp_write((sample_count >> 8) & 0xFF);

  // arch_enable_ints();

  printk("%d\n", sb16_wq.wait().interrupted());
  return sz;
}

static int sb16_open(fs::file &fd) {
  printk("here\n");
  return 0;
}

struct fs::file_operations sb_ops = {
    .write = sb16_write,
    .open = sb16_open,
};


static struct dev::driver_info sb16_driver {
  .name = "sb16", .type = DRIVER_CHAR, .major = MAJOR_SB16,

  .char_ops = &sb_ops,
};


static void sb16_interrupt(int intr, reg_t *fr, void *) {
  printk(KERN_INFO "SB16 INTERRUPT\n");
  // Stop sound output ready for the next block.
  dsp_write(0xd5);

  inb(DSP_STATUS);                           // 8 bit interrupt
  if (m_major_version >= 4) inb(DSP_R_ACK);  // 16 bit interrupt

  sb16_wq.wake_up_all();

  irq::eoi(intr);
}


void sb16_init(void) {
  // Try to detect the soundblaster 16 card
  outb(0x226, 1);
  delay(32);
  outb(0x226, 0);

  auto data = dsp_read();
  if (data != 0xAA) {
    // Soundblaster was not found, just return
    return;
  }

  // Turn the speaker on
  dsp_write(0xD1);

  // Get the version info
  dsp_write(0xe1);
  m_major_version = dsp_read();
  auto vmin = dsp_read();


  printk(KERN_INFO "SB16: found version %d.%d\n", m_major_version, vmin);
  set_irq_register(SB16_DEFAULT_IRQ);
  printk(KERN_INFO "SB16: IRQ %d\n", get_irq_line());

  irq::install(get_irq_line(), sb16_interrupt, "Sound Blaster 16");
  // finally initialize
  dev::register_driver(sb16_driver);

  dev::register_name(sb16_driver, "sb16", 0);
}

module_init("sb16", sb16_init);
