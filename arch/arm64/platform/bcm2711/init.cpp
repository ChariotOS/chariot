#include <arch.h>
#include <printk.h>
#include <arm64/arch.h>
#include <arm64/platform/bcm2711.h>
#include <arm64/platform/mailbox.h>



////////////////////////////////////////////////////////////////


// GPIO

#define PERIPHERAL_BASE (0xFE000000)
#define GPFSEL0 (PERIPHERAL_BASE + 0x200000)
#define GPSET0 (PERIPHERAL_BASE + 0x20001C)
#define GPCLR0 (PERIPHERAL_BASE + 0x200028)
#define GPPUPPDN0 (PERIPHERAL_BASE + 0x2000E4)

#define GPIO_MAX_PIN 53
#define GPIO_FUNCTION_ALT5 2

enum {
  Pull_None = 0,
};



unsigned int gpio_call(unsigned int pin_number, unsigned int value, unsigned int base, unsigned int field_size, unsigned int field_max) {
  unsigned int field_mask = (1 << field_size) - 1;

  if (pin_number > field_max) return 0;
  if (value > field_mask) return 0;

  unsigned int num_fields = 32 / field_size;
  unsigned int reg = base + ((pin_number / num_fields) * 4);
  unsigned int shift = (pin_number % num_fields) * field_size;

  unsigned int curval = arm64::read(reg);
  curval &= ~(field_mask << shift);
  curval |= value << shift;
  arm64::write(reg, curval);

  return 1;
}

unsigned int gpio_set(unsigned int pin_number, unsigned int value) { return gpio_call(pin_number, value, GPSET0, 1, GPIO_MAX_PIN); }
unsigned int gpio_clear(unsigned int pin_number, unsigned int value) { return gpio_call(pin_number, value, GPCLR0, 1, GPIO_MAX_PIN); }
unsigned int gpio_pull(unsigned int pin_number, unsigned int value) { return gpio_call(pin_number, value, GPPUPPDN0, 2, GPIO_MAX_PIN); }
unsigned int gpio_function(unsigned int pin_number, unsigned int value) { return gpio_call(pin_number, value, GPFSEL0, 3, GPIO_MAX_PIN); }

void gpio_useAsAlt5(unsigned int pin_number) {
  gpio_pull(pin_number, Pull_None);
  gpio_function(pin_number, GPIO_FUNCTION_ALT5);
}

// UART




#define AUX_BASE (PERIPHERAL_BASE + 0x215000)
#define AUX_ENABLES (AUX_BASE + 4)
#define AUX_MU_IO_REG (AUX_BASE + 64)
#define AUX_MU_IER_REG (AUX_BASE + 68)
#define AUX_MU_IIR_REG (AUX_BASE + 72)
#define AUX_MU_LCR_REG (AUX_BASE + 76)
#define AUX_MU_MCR_REG (AUX_BASE + 80)
#define AUX_MU_LSR_REG (AUX_BASE + 84)
#define AUX_MU_CNTL_REG (AUX_BASE + 96)
#define AUX_MU_BAUD_REG (AUX_BASE + 104)
#define AUX_UART_CLOCK (500000000)
#define UART_MAX_QUEUE (16 * 1024)
#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK / (baud * 8)) - 1)


void serial_install(void) {
  arm64::write(AUX_ENABLES, 1);  // enable UART1
  arm64::write(AUX_MU_IER_REG, 0);
  arm64::write(AUX_MU_CNTL_REG, 0);
  arm64::write(AUX_MU_LCR_REG, 3);  // 8 bits
  arm64::write(AUX_MU_MCR_REG, 0);
  arm64::write(AUX_MU_IER_REG, 0);
  arm64::write(AUX_MU_IIR_REG, 0xC6);  // disable interrupts
  arm64::write(AUX_MU_BAUD_REG, AUX_MU_BAUD(115200));
  // gpio_useAsAlt5(14);
  // gpio_useAsAlt5(15);
  arm64::write(AUX_MU_CNTL_REG, 3);  // enable RX/TX
}

void serial_send(int port, char ch) {
	while (!(arm64::read(AUX_MU_LSR_REG) & 0x20)) {}

	arm64::write(AUX_MU_IO_REG, (unsigned int)ch);
}


////////////////////////////////////////////////////////////////

void arm64::platform_init(uint64_t dtb, uint64_t x1, uint64_t x2, uint64_t x3) {
  /* initialize the serial port */
  serial_install();


	/* Do some printing */
	printk("hello, world\n");

  while (1) {
  }

  serial_string(0, (char *)"Hello, world\n");
  while (1) {
  }
}
