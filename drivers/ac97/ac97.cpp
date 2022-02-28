#include <asm.h>
#include <cpu.h>
#include <dev/driver.h>
#include <errno.h>
#include <module.h>
#include <phys.h>
#include <printk.h>
#include <util.h>
#include <pci.h>

#include "../majors.h"



/* Utility macros */
#define N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

/* BARs! */
#define AC97_NAMBAR 0x10  /* Native Audio Mixer Base Address Register */
#define AC97_NABMBAR 0x14 /* Native Audio Bus Mastering Base Address Register */

/* Bus mastering IO port offsets */
#define AC97_PO_BDBAR 0x10 /* PCM out buffer descriptor BAR */
#define AC97_PO_CIV 0x14   /* PCM out current index value */
#define AC97_PO_LVI 0x15   /* PCM out last valid index */
#define AC97_PO_SR 0x16    /* PCM out status register */
#define AC97_PO_PICB 0x18  /* PCM out position in current buffer register */
#define AC97_PO_CR 0x1B    /* PCM out control register */

/* Bus mastering misc */
/* Buffer descriptor list constants */
#define AC97_BDL_LEN 32                               /* Buffer descriptor list length */
#define AC97_BDL_BUFFER_LEN 0x1000                    /* Length of buffer in BDL */
#define AC97_CL_GET_LENGTH(cl) ((cl)&0xFFFF)          /* Decode length from cl */
#define AC97_CL_SET_LENGTH(cl, v) ((cl) = (v)&0xFFFF) /* Encode length to cl */
#define AC97_CL_BUP ((uint32_t)1 << 30)               /* Buffer underrun policy in cl */
#define AC97_CL_IOC ((uint32_t)1 << 31)               /* Interrupt on completion flag in cl */

/* PCM out control register flags */
#define AC97_X_CR_RPBM (1 << 0)  /* Run/pause bus master */
#define AC97_X_CR_RR (1 << 1)    /* Reset registers */
#define AC97_X_CR_LVBIE (1 << 2) /* Last valid buffer interrupt enable */
#define AC97_X_CR_FEIE (1 << 3)  /* FIFO error interrupt enable */
#define AC97_X_CR_IOCE (1 << 4)  /* Interrupt on completion enable */

/* Status register flags */
#define AC97_X_SR_DCH (1 << 0)   /* DMA controller halted */
#define AC97_X_SR_CELV (1 << 1)  /* Current equals last valid */
#define AC97_X_SR_LVBCI (1 << 2) /* Last valid buffer completion interrupt */
#define AC97_X_SR_BCIS (1 << 3)  /* Buffer completion interrupt status */
#define AC97_X_SR_FIFOE (1 << 4) /* FIFO error */

/* Mixer IO port offsets */
#define AC97_RESET 0x00
#define AC97_MASTER_VOLUME 0x02
#define AC97_AUX_OUT_VOLUME 0x04
#define AC97_MONO_VOLUME 0x06
#define AC97_PCM_OUT_VOLUME 0x18

/* snd values */
#define AC97_SND_NAME "Intel AC'97"
#define AC97_PLAYBACK_SPEED 48000
#define AC97_PLAYBACK_FORMAT SND_FORMAT_L16SLE




static spinlock ac97_lock;
static void *dma_page = NULL;
static wait_queue ac97_wq;


/* An entry in a buffer dscriptor list */
typedef struct {
  uint32_t pointer; /* Pointer to buffer */
  uint32_t cl;      /* Control values and buffer length */
} __attribute__((packed)) ac97_bdl_entry_t;

typedef struct {
  pci::device *pci_device;
  uint16_t nabmbar;             /* Native audio bus mastring BAR */
  uint16_t nambar;              /* Native audio mixing BAR */
  size_t irq;                   /* This ac97's irq */
  uint8_t lvi;                  /* The currently set last valid index */
  uint8_t bits;                 /* How many bits of volume are supported (5 or 6) */
  ac97_bdl_entry_t *bdl;        /* Buffer descriptor list */
  uint16_t *bufs[AC97_BDL_LEN]; /* Virtual addresses for buffers in BDL */
  uint32_t bdl_p;
  uint32_t mask;
} ac97_device_t;

static ac97_device_t _device;

static ssize_t ac97_write(fs::File &fd, const char *buf, size_t sz) {
  scoped_lock l(ac97_lock);
  return sz;
}

static int ac97_open(fs::File &fd) { return 0; }

static void ac97_interrupt(int intr, reg_t *fr, void *) {
  printk(KERN_INFO "ac97 INTERRUPT\n");
  /* TODO: wake waiters up! */
  irq::eoi(intr);
}

void ac97_pci_init(pci::device *dev) {
  _device.pci_device = dev;
  // _device.nabmbar = pci_read_field(_device.pci_device, AC97_NABMBAR, 2) & ((uint32_t) -1) << 1;
  // _device.nambar = pci_read_field(_device.pci_device, PCI_BAR0, 4) & ((uint32_t) -1) << 1;
  // _device.irq = pci_get_interrupt(_device.pci_device);
}

void ac97_init(void) {
  // printk("==================================\n");
  pci::walk_devices([](pci::device *dev) {
    if (dev->vendor_id == 0x8086 && dev->device_id == 0x2415) {
      ac97_pci_init(dev);
      return;
    }
  });
#if 0
  irq::install(get_irq_line(), ac97_interrupt, "Sound Blaster 16");
  // finally initialize
  dev::register_driver(ac97_driver);

  dev::register_name(ac97_driver, "ac97", 0);
#endif
}

module_init("ac97", ac97_init);
