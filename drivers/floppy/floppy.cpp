#include <asm.h>
#include <cpu.h>
#include <printk.h>
#include <util.h>
#include <module.h>
#include <sched.h>

#define FDC_DMA_CHANNEL 2
#define FLPY_SECTORS_PER_TRACK 18
#define DMA_BUFFER 0x1000



/**
 *        2 DMACs, 32 bit master & 16bit slave each having 8 channels
 */
#define DMA_MAX_CHANNELS 16
#define DMA_CHANNELS_PER_DMAC 8

/**
 *        DMA0 address/count registers
 */
enum DMA0_CHANNEL_IO {

  DMA0_CHAN0_ADDR_REG = 0,  // Thats right, i/o port 0
  DMA0_CHAN0_COUNT_REG = 1,
  DMA0_CHAN1_ADDR_REG = 2,
  DMA0_CHAN1_COUNT_REG = 3,
  DMA0_CHAN2_ADDR_REG = 4,
  DMA0_CHAN2_COUNT_REG = 5,
  DMA0_CHAN3_ADDR_REG = 6,
  DMA0_CHAN3_COUNT_REG = 7,
};

/**
 *        Generic DMA0 registers
 */
enum DMA0_IO {

  DMA0_STATUS_REG = 0x08,
  DMA0_COMMAND_REG = 0x08,
  DMA0_REQUEST_REG = 0x09,
  DMA0_CHANMASK_REG = 0x0a,
  DMA0_MODE_REG = 0x0b,
  DMA0_CLEARBYTE_FLIPFLOP_REG = 0x0c,
  DMA0_TEMP_REG = 0x0d,
  DMA0_MASTER_CLEAR_REG = 0x0d,
  DMA0_CLEAR_MASK_REG = 0x0e,
  DMA0_MASK_REG = 0x0f
};

/**
 *        DMA1 address/count registers
 */
enum DMA1_CHANNEL_IO {

  DMA1_CHAN4_ADDR_REG = 0xc0,
  DMA1_CHAN4_COUNT_REG = 0xc2,
  DMA1_CHAN5_ADDR_REG = 0xc4,
  DMA1_CHAN5_COUNT_REG = 0xc6,
  DMA1_CHAN6_ADDR_REG = 0xc8,
  DMA1_CHAN6_COUNT_REG = 0xca,
  DMA1_CHAN7_ADDR_REG = 0xcc,
  DMA1_CHAN7_COUNT_REG = 0xce,
};

/**
 *        DMA External Page Registers
 */
enum DMA0_PAGE_REG {

  DMA_PAGE_EXTRA0 = 0x80,  // Also diagnostics port
  DMA_PAGE_CHAN2_ADDRBYTE2 = 0x81,
  DMA_PAGE_CHAN3_ADDRBYTE2 = 0x82,
  DMA_PAGE_CHAN1_ADDRBYTE2 = 0x83,
  DMA_PAGE_EXTRA1 = 0x84,
  DMA_PAGE_EXTRA2 = 0x85,
  DMA_PAGE_EXTRA3 = 0x86,
  DMA_PAGE_CHAN6_ADDRBYTE2 = 0x87,
  DMA_PAGE_CHAN7_ADDRBYTE2 = 0x88,
  DMA_PAGE_CHAN5_ADDRBYTE2 = 0x89,
  DMA_PAGE_EXTRA4 = 0x8c,
  DMA_PAGE_EXTRA5 = 0x8d,
  DMA_PAGE_EXTRA6 = 0x8e,
  DMA_PAGE_DRAM_REFRESH = 0x8f  // no longer used in new PCs
};

/**
 *        Generic DMA1 registers
 */
enum DMA1_IO {

  DMA1_STATUS_REG = 0xd0,
  DMA1_COMMAND_REG = 0xd0,
  DMA1_REQUEST_REG = 0xd2,
  DMA1_CHANMASK_REG = 0xd4,
  DMA1_MODE_REG = 0xd6,
  DMA1_CLEARBYTE_FLIPFLOP_REG = 0xd8,
  DMA1_INTER_REG = 0xda,
  DMA1_UNMASK_ALL_REG = 0xdc,
  DMA1_MASK_REG = 0xde
};

/**
 *        DMA mode bit mask
 */

enum DMA_MODE_REG_MASK {

  DMA_MODE_MASK_SEL = 3,

  DMA_MODE_MASK_TRA = 0xc,
  DMA_MODE_SELF_TEST = 0,
  DMA_MODE_READ_TRANSFER = 4,
  DMA_MODE_WRITE_TRANSFER = 8,

  DMA_MODE_MASK_AUTO = 0x10,
  DMA_MODE_MASK_IDEC = 0x20,

  DMA_MODE_MASK = 0xc0,
  DMA_MODE_TRANSFER_ON_DEMAND = 0,
  DMA_MODE_TRANSFER_SINGLE = 0x40,
  DMA_MODE_TRANSFER_BLOCK = 0x80,
  DMA_MODE_TRANSFER_CASCADE = 0xC0
};

enum DMA_CMD_REG_MASK {

  DMA_CMD_MASK_MEMTOMEM = 1,
  DMA_CMD_MASK_CHAN0ADDRHOLD = 2,
  DMA_CMD_MASK_ENABLE = 4,
  DMA_CMD_MASK_TIMING = 8,
  DMA_CMD_MASK_PRIORITY = 0x10,
  DMA_CMD_MASK_WRITESEL = 0x20,
  DMA_CMD_MASK_DREQ = 0x40,
  DMA_CMD_MASK_DACK = 0x80
};



void dma_mask_channel(uint8_t channel) {
  if (channel <= 4)
    outb(DMA0_CHANMASK_REG, (1 << (channel - 1)));
  else
    outb(DMA1_CHANMASK_REG, (1 << (channel - 5)));
}

// unmasks a channel
void dma_unmask_channel(uint8_t channel) {
  if (channel <= 4)
    outb(DMA0_CHANMASK_REG, channel);
  else
    outb(DMA1_CHANMASK_REG, channel);
}

// unmasks all channels
void dma_unmask_all(int dma) {
  // it doesnt matter what is written to this register
  outb(DMA1_UNMASK_ALL_REG, 0xff);
}

// resets controller to defaults
void dma_reset(int dma) {
  // it doesnt matter what is written to this register
  outb(DMA0_TEMP_REG, 0xff);
}

// resets flipflop
void dma_reset_flipflop(int dma) {
  if (dma < 2) return;

  // it doesnt matter what is written to this register
  outb((dma == 0) ? (int)DMA0_CLEARBYTE_FLIPFLOP_REG : (int)DMA1_CLEARBYTE_FLIPFLOP_REG, 0xff);
}

// sets the address of a channel
void dma_set_address(uint8_t channel, uint8_t low, uint8_t high) {
  if (channel > 8) return;

  unsigned short port = 0;
  switch (channel) {
    case 0: {
      port = DMA0_CHAN0_ADDR_REG;
      break;
    }
    case 1: {
      port = DMA0_CHAN1_ADDR_REG;
      break;
    }
    case 2: {
      port = DMA0_CHAN2_ADDR_REG;
      break;
    }
    case 3: {
      port = DMA0_CHAN3_ADDR_REG;
      break;
    }
    case 4: {
      port = DMA1_CHAN4_ADDR_REG;
      break;
    }
    case 5: {
      port = DMA1_CHAN5_ADDR_REG;
      break;
    }
    case 6: {
      port = DMA1_CHAN6_ADDR_REG;
      break;
    }
    case 7: {
      port = DMA1_CHAN7_ADDR_REG;
      break;
    }
  }

  outb(port, low);
  outb(port, high);
}

// sets the counter of a channel
void dma_set_count(uint8_t channel, uint8_t low, uint8_t high) {
  if (channel > 8) return;

  unsigned short port = 0;
  switch (channel) {
    case 0: {
      port = DMA0_CHAN0_COUNT_REG;
      break;
    }
    case 1: {
      port = DMA0_CHAN1_COUNT_REG;
      break;
    }
    case 2: {
      port = DMA0_CHAN2_COUNT_REG;
      break;
    }
    case 3: {
      port = DMA0_CHAN3_COUNT_REG;
      break;
    }
    case 4: {
      port = DMA1_CHAN4_COUNT_REG;
      break;
    }
    case 5: {
      port = DMA1_CHAN5_COUNT_REG;
      break;
    }
    case 6: {
      port = DMA1_CHAN6_COUNT_REG;
      break;
    }
    case 7: {
      port = DMA1_CHAN7_COUNT_REG;
      break;
    }
  }

  outb(port, low);
  outb(port, high);
}

void dma_set_mode(uint8_t channel, uint8_t mode) {
  int dma = (channel < 4) ? 0 : 1;
  int chan = (dma == 0) ? channel : channel - 4;

  dma_mask_channel(channel);
  outb((channel < 4) ? (int)(DMA0_MODE_REG) : (int)DMA1_MODE_REG, chan | (mode));
  dma_unmask_all(dma);
}

// prepares channel for read
void dma_set_read(uint8_t channel) { dma_set_mode(channel, DMA_MODE_READ_TRANSFER | DMA_MODE_TRANSFER_SINGLE); }

// prepares channel for write
void dma_set_write(uint8_t channel) { dma_set_mode(channel, DMA_MODE_WRITE_TRANSFER | DMA_MODE_TRANSFER_SINGLE); }

// writes to an external page register
void dma_set_external_page_register(uint8_t reg, uint8_t val) {
  if (reg > 14) return;

  unsigned short port = 0;
  switch (reg) {
    case 1: {
      port = DMA_PAGE_CHAN1_ADDRBYTE2;
      break;
    }
    case 2: {
      port = DMA_PAGE_CHAN2_ADDRBYTE2;
      break;
    }
    case 3: {
      port = DMA_PAGE_CHAN3_ADDRBYTE2;
      break;
    }
    case 4: {
      return;
    }  // nothing should ever write to register 4
    case 5: {
      port = DMA_PAGE_CHAN5_ADDRBYTE2;
      break;
    }
    case 6: {
      port = DMA_PAGE_CHAN6_ADDRBYTE2;
      break;
    }
    case 7: {
      port = DMA_PAGE_CHAN7_ADDRBYTE2;
      break;
    }
  }

  outb(port, val);
}


/* Controller I/O ports */
enum FLPYDSK_IO {

  FLPYDSK_DOR = 0x3f2,
  FLPYDSK_MSR = 0x3f4,
  FLPYDSK_FIFO = 0x3f5,
  FLPYDSK_CTRL = 0x3f7
};
/* b0-4 of command */
enum FLPYDSK_CMD {

  FDC_CMD_READ_TRACK = 2,
  FDC_CMD_SPECIFY = 3,
  FDC_CMD_CHECK_STAT = 4,
  FDC_CMD_WRITE_SECT = 5,
  FDC_CMD_READ_SECT = 6,
  FDC_CMD_CALIBRATE = 7,
  FDC_CMD_CHECK_INT = 8,
  FDC_CMD_FORMAT_TRACK = 0xd,
  FDC_CMD_SEEK = 0xf
};

enum FLPYDSK_CMD_EXT {

  FDC_CMD_EXT_SKIP = 0x20,       // 00100000
  FDC_CMD_EXT_DENSITY = 0x40,    // 01000000
  FDC_CMD_EXT_MULTITRACK = 0x80  // 10000000
};

/* digital output */

enum FLPYDSK_DOR_MASK {

  FLPYDSK_DOR_MASK_DRIVE0 = 0,         // 00000000        = here for completeness sake
  FLPYDSK_DOR_MASK_DRIVE1 = 1,         // 00000001
  FLPYDSK_DOR_MASK_DRIVE2 = 2,         // 00000010
  FLPYDSK_DOR_MASK_DRIVE3 = 3,         // 00000011
  FLPYDSK_DOR_MASK_RESET = 4,          // 00000100
  FLPYDSK_DOR_MASK_DMA = 8,            // 00001000
  FLPYDSK_DOR_MASK_DRIVE0_MOTOR = 16,  // 00010000
  FLPYDSK_DOR_MASK_DRIVE1_MOTOR = 32,  // 00100000
  FLPYDSK_DOR_MASK_DRIVE2_MOTOR = 64,  // 01000000
  FLPYDSK_DOR_MASK_DRIVE3_MOTOR = 128  // 10000000
};

/* main status */


enum FLPYDSK_MSR_MASK {

  FLPYDSK_MSR_MASK_DRIVE1_POS_MODE = 1,  // 00000001
  FLPYDSK_MSR_MASK_DRIVE2_POS_MODE = 2,  // 00000010
  FLPYDSK_MSR_MASK_DRIVE3_POS_MODE = 4,  // 00000100
  FLPYDSK_MSR_MASK_DRIVE4_POS_MODE = 8,  // 00001000
  FLPYDSK_MSR_MASK_BUSY = 16,            // 00010000
  FLPYDSK_MSR_MASK_DMA = 32,             // 00100000
  FLPYDSK_MSR_MASK_DATAIO = 64,          // 01000000
  FLPYDSK_MSR_MASK_DATAREG = 128         // 10000000
};

/* controller status port 0 */
enum FLPYDSK_ST0_MASK {

  FLPYDSK_ST0_MASK_DRIVE0 = 0,      // 00000000
  FLPYDSK_ST0_MASK_DRIVE1 = 1,      // 00000001
  FLPYDSK_ST0_MASK_DRIVE2 = 2,      // 00000010
  FLPYDSK_ST0_MASK_DRIVE3 = 3,      // 00000011
  FLPYDSK_ST0_MASK_HEADACTIVE = 4,  // 00000100
  FLPYDSK_ST0_MASK_NOTREADY = 8,    // 00001000
  FLPYDSK_ST0_MASK_UNITCHECK = 16,  // 00010000
  FLPYDSK_ST0_MASK_SEEKEND = 32,    // 00100000
  FLPYDSK_ST0_MASK_INTCODE = 64     // 11000000
};

/* INTCODE types */
enum FLPYDSK_ST0_INTCODE_TYP {

  FLPYDSK_ST0_TYP_NORMAL = 0,
  FLPYDSK_ST0_TYP_ABNORMAL_ERR = 1,
  FLPYDSK_ST0_TYP_INVALID_ERR = 2,
  FLPYDSK_ST0_TYP_NOTREADY = 3
};

/* third gap sizes */
enum FLPYDSK_GAP3_LENGTH {

  FLPYDSK_GAP3_LENGTH_STD = 42,
  FLPYDSK_GAP3_LENGTH_5_14 = 32,
  FLPYDSK_GAP3_LENGTH_3_5 = 27
};

/* DTL size */
enum FLPYDSK_SECTOR_DTL {

  FLPYDSK_SECTOR_DTL_128 = 0,
  FLPYDSK_SECTOR_DTL_256 = 1,
  FLPYDSK_SECTOR_DTL_512 = 2,
  FLPYDSK_SECTOR_DTL_1024 = 4
};



static bool primary_avail = false;
static bool secondary_avail = false;
static uint8_t irq_h = 0;
/*
void floppy_init(void) {

}
*/


void flpy_irq(int irq, reg_t *reg, void *) {
  printk_nolock("floppy irq!\n");
  irq_h = 1;
}



inline uint8_t read_status() { return inb(FLPYDSK_MSR); }

void write_dor(uint8_t cmd) { outb(FLPYDSK_DOR, cmd); }

void write_cmd(uint8_t cmd) {
  uint8_t timeout = 0xff;
  while (--timeout) {
    if (read_status() & FLPYDSK_MSR_MASK_DATAREG) {
      outb(FLPYDSK_FIFO, cmd);
      return;
    }
  }
  printk("FATAL: timeout durint %s\n", __func__);
}

uint8_t read_data() {
  for (int i = 0; i < 500; i++)
    if (read_status() & FLPYDSK_MSR_MASK_DATAREG) return inb(FLPYDSK_FIFO);
  return 0;
}

void write_ccr(uint8_t cmd) { outb(FLPYDSK_CTRL, cmd); }

void fdc_disable_controller() { write_dor(0); }

void fdc_enable_controller() { write_dor(FLPYDSK_DOR_MASK_RESET | FLPYDSK_DOR_MASK_DMA); }

void fdc_dma_init(uint8_t *buffer, uint32_t length) {
  union {
    uint8_t byte[4];
    unsigned long l;
  } a, c;
  a.l = (long)buffer;
  c.l = (long)length - 1;

  dma_reset(1);
  dma_mask_channel(FDC_DMA_CHANNEL);  // Mask channel 2
  dma_reset_flipflop(1);              // Flipflop reset on DMA 1

  dma_set_address(FDC_DMA_CHANNEL, a.byte[0], a.byte[1]);  // Buffer address
  dma_reset_flipflop(1);                                   // Flipflop reset on DMA 1

  dma_set_count(FDC_DMA_CHANNEL, c.byte[0], c.byte[1]);  // Set count
  dma_set_read(FDC_DMA_CHANNEL);

  dma_unmask_all(1);  // Unmask channel 2
}

void fdc_check_int(uint32_t *st0, uint32_t *cy1) {
  write_cmd(FDC_CMD_CHECK_INT);
  *st0 = read_data();
  *cy1 = read_data();
}

uint8_t wait_for_irq() {
  uint32_t timeout = 10;
  uint8_t ret = 0;
  while (!irq_h) {
    // if (!timeout) break;
    timeout--;
    // sched::yield();
  }
  if (irq_h) ret = 1;
  irq_h = 0;
  return ret;
}

void fdc_set_motor(uint8_t drive, uint8_t status) {
  if (drive > 3) {
    printk("Invalid drive selected.\n");
    return;
  }
  uint8_t motor = 0;
  switch (drive) {
    case 0:
      motor = FLPYDSK_DOR_MASK_DRIVE0_MOTOR;
      break;
    case 1:
      motor = FLPYDSK_DOR_MASK_DRIVE1_MOTOR;
      break;
    case 2:
      motor = FLPYDSK_DOR_MASK_DRIVE2_MOTOR;
      break;
    case 3:
      motor = FLPYDSK_DOR_MASK_DRIVE3_MOTOR;
      break;
  }
  if (status) {
    write_dor(drive | motor | FLPYDSK_DOR_MASK_RESET | FLPYDSK_DOR_MASK_DMA);
  } else {
    write_dor(FLPYDSK_DOR_MASK_RESET);
  }
}

void fdc_drive_set(uint8_t step, uint8_t loadt, uint8_t unloadt, uint8_t dma) {
  printk("%s: steprate: %dms, load: %dms, unload: %dms, dma:%d\n", __func__, step, loadt, unloadt, dma);
  write_cmd(FDC_CMD_SPECIFY);
  uint8_t data = 0;
  data = ((step & 0xf) << 4) | (unloadt & 0xf);
  write_cmd(data);
  data = ((loadt << 1) | (dma ? 0 : 1));
  write_cmd(data);
}

void fdc_calibrate(uint8_t drive) {
  uint32_t st0, cy1;

  fdc_set_motor(drive, 1);
  for (int i = 0; i < 10; i++) {
    write_cmd(FDC_CMD_CALIBRATE);
    write_cmd(drive);
    if (!wait_for_irq()) {
      goto fail;
    }
    fdc_check_int(&st0, &cy1);
    if (!cy1) {
      fdc_set_motor(drive, 0);
      printk("Calibration successful!\n");
      return;
    }
  }
fail:
  fdc_set_motor(drive, 0);
  printk("Unable to calibrate!\n");
}




inline void parse_cmos(uint8_t primary, uint8_t secondary) {
  if (primary != 0) {
    primary_avail = 1;
    printk("Primary FDC channel available.\n");
  }
  if (secondary != 0) {
    secondary_avail = 1;
    printk("Secondary FDC channel available.\n");
  }
}

void fdc_reset() {
  fdc_disable_controller();
  fdc_enable_controller();
  if (!wait_for_irq()) {
    printk("FATAL: IRQ link offline. %s\n", __func__);
    return;
  }
  printk("IRQ link is online.\n");
  uint32_t st0, cy1;
  for (int i = 0; i < 4; i++)
    fdc_check_int(&st0, &cy1);

  printk("Setting transfer speed to 500Kb/s\n");
  write_ccr(0);

  fdc_calibrate(0);

  fdc_drive_set(3, 16, 240, 1);

  /*
device_t *floppy = (device_t *)malloc(sizeof(floppy));
floppy->name = "FLOPPY";
floppy->unique_id = 0x13;
floppy->dev_type = DEVICE_BLOCK;
floppy->read = flpy_read;
floppy->write = flpy_write;
device_add(floppy);
  */
}



uint8_t fdc_seek(uint8_t cyl, uint8_t head) {
  uint32_t st0, cy10;

  for (int i = 0; i < 10; i++) {
    write_cmd(FDC_CMD_SEEK);
    write_cmd((head << 2) | 0);
    write_cmd(cyl);

    if (!wait_for_irq()) {
      printk("FATAL: IRQ link offline! %s\n", __func__);
      return 1;
    }
    fdc_check_int(&st0, &cy10);
    if (cy10 == cyl) return 0;
  }
  return 1;
}

uint8_t flpy_read_sector(uint8_t head, uint8_t track, uint8_t sector) {
  uint32_t st0, cy1;

  fdc_dma_init((uint8_t *)DMA_BUFFER, 512);
  dma_set_read(FDC_DMA_CHANNEL);
  write_cmd(FDC_CMD_READ_SECT | (int)FDC_CMD_EXT_MULTITRACK | FDC_CMD_EXT_SKIP | FDC_CMD_EXT_DENSITY);
  write_cmd((head << 2) | 0);
  write_cmd(track);
  write_cmd(head);
  write_cmd(sector);
  write_cmd(FLPYDSK_SECTOR_DTL_512);
  write_cmd(((sector + 1) >= FLPY_SECTORS_PER_TRACK) ? FLPY_SECTORS_PER_TRACK : (sector + 1));
  write_cmd(FLPYDSK_GAP3_LENGTH_3_5);
  write_cmd(0xff);

  if (!wait_for_irq()) {
    printk("FATAL: IRQ link offline! %s\n", __func__);
    return 1;
  }
  for (int j = 0; j < 7; j++)
    read_data();

  fdc_check_int(&st0, &cy1);
  return 1;
}

void lba_to_chs(int lba, int *head, int *track, int *sector) {
  *head = (lba % (FLPY_SECTORS_PER_TRACK * 2)) / FLPY_SECTORS_PER_TRACK;
  *track = lba / (FLPY_SECTORS_PER_TRACK * 2);
  *sector = lba % FLPY_SECTORS_PER_TRACK + 1;
}

uint8_t *flpy_read_lba(int lba) {
  int head = 0, track = 0, sector = 1;
  int rc = 0;
  lba_to_chs(lba, &head, &track, &sector);

  fdc_set_motor(0, 1);
  rc = fdc_seek(track, head);
  if (rc) {
    printk("Failed to seek!\n");
    return 0;
  }

  flpy_read_sector(head, track, sector);
  fdc_set_motor(0, 0);

  return (uint8_t *)p2v(DMA_BUFFER);
}


uint8_t flpy_read(uint8_t *buffer, uint32_t lba, uint32_t sectors) {
  if (!sectors) return 1;
  uint32_t sectors_read = 0;
  while (sectors_read != sectors) {
    uint8_t *buf = flpy_read_lba(lba + sectors_read);
    if (!buf) return 1;
    memcpy(buffer + sectors_read * 512, buf, 512);
    sectors_read++;
  }
  return 0;
}


void floppy_init(void) {
  // detect
  outb(0x70, 0x10);
  uint8_t cmos = inb(0x71);
  parse_cmos((cmos & 0xf0) >> 4, cmos & 0x0f);
  if (primary_avail) {
    irq::install(38, flpy_irq, "floppy", NULL);
    fdc_reset();
  }
}

module_init("floppy", floppy_init);


ksh_def("floppy", "detect floppy drives") {
  uint8_t buf[512];
  memset(buf, 0xFE, 512);

  flpy_read(buf, 0, 1);
  hexdump(buf, 512, true);
  return 0;
}

ksh_def("fdc_read", "read a sector") {


  if (args.size() != 1) {
    printk("Usage: fdc_read <sector>\n");
    return 1;
	}


	uint8_t buf[512];
	int lba = 0;


	memset(buf, 0xA0, 512);

	args[0].scan("%d", &lba);
	flpy_read(buf, lba, 1);

	hexdump(buf, 512, true);

  return 0;
}
