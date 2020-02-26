#include <asm.h>
#include <cpu.h>
#include <dev/driver.h>
#include <module.h>
#include <phys.h>
#include <printk.h>
#include <util.h>

#include "../majors.h"

// Digital Sound Processor ports
#define DSP_RESET 0x226
#define DSP_READ 0x22A
#define DSP_WRITE 0x22C
#define DSP_BUFFER 0x22A
#define DSP_STATUS 0x22E
#define DSP_INTERRUPT 0x22F

// Digital Sound Processor commands
#define DSP_CMD_OUTPUT_RATE 0x41
#define DSP_CMD_TRANSFER_MODE 0xB6
#define DSP_CMD_STOP 0xD5
#define DSP_CMD_VERSION 0xE1

// DMA ports
#define DMA_ADDRES 0xC4
#define DMA_COUNT 0xC6
#define DMA_PAGE 0x8B
#define DMA_SINGLE_MASK 0xD4
#define DMA_TRANSFER_MODE 0xD6
#define DMA_CLEAR_POINTER 0xD8

inline void delay_3us() {
  // ~3 microsecs
  for (auto i = 0; i < 32; i++) {
    inb(0x80);
  }
}

int version_major = 0;
int version_minor = 0;

/* Write a value to the DSP write register */
static void dsp_write(u8 value) {
  while (inb(DSP_WRITE) & 0x80) {
    ;
  }
  outb(DSP_WRITE, value);
}

/* Reads the value of the DSP read register */
static u8 dsp_read() {
  while (!(inb(DSP_STATUS) & 0x80)) {
    ;
  }
  return inb(DSP_READ);
}

/* Changes the sample rate of sound output */
/*
static void set_sample_rate(uint16_t hz) {
  dsp_write(0x41);  // output
  dsp_write((u8)(hz >> 8));
  dsp_write((u8)hz);
  dsp_write(0x42);  // input
  dsp_write((u8)(hz >> 8));
  dsp_write((u8)hz);
}
*/

void *dma_page = NULL;

/*
static void dma_start(uint32_t len) {
  if (dma_page == NULL) {
    dma_page = phys::alloc();
  }
}
*/

static ssize_t sb16_write(fs::filedesc &fd, const char *buf, size_t sz) {
  hexdump((void *)buf, sz);
  return -1;
}

static int sb16_open(fs::filedesc &fd) {
  printk("here\n");
  return -1;
}

struct dev::driver_ops sb_ops = {
    .write = sb16_write,
    .open = sb16_open,
};

void sb16_init(void) {
  outb(0x226, 1);
  delay_3us();
  outb(0x226, 0);

  auto data = dsp_read();
  if (data == 0xaa) {
    // Get the version info
    dsp_write(0xe1);
    version_major = dsp_read();
    version_minor = dsp_read();
    printk("SB16: found version %d.%d\n", version_major, version_minor);
  } else {
    printk("SB16: sb not ready");
  }

  // finally initialize
  dev::register_driver("sb16", CHAR_DRIVER, MAJOR_SB16, &sb_ops);
}

// module_init("sb16", sb16_init);
