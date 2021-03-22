#pragma once

/* The virtio input device can be used to create virtual human interface devices such as keyboards,
 * mice and tablets. An instance of the virtio device represents one such input device.
 */


#include <dev/virtio/mmio.h>

/* from linux */
#define INPUT_PROP_POINTER 0x00        /* needs a pointer */
#define INPUT_PROP_DIRECT 0x01         /* direct input devices */
#define INPUT_PROP_BUTTONPAD 0x02      /* has button(s) under pad */
#define INPUT_PROP_SEMI_MT 0x03        /* touch rectangle only */
#define INPUT_PROP_TOPBUTTONPAD 0x04   /* softbuttons at top of pad */
#define INPUT_PROP_POINTING_STICK 0x05 /* is a pointing stick */
#define INPUT_PROP_ACCELEROMETER 0x06  /* has accelerometer */

#define INPUT_PROP_MAX 0x1f

/*
 * Event types
 */
#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03
#define EV_MSC 0x04
#define EV_SW 0x05
#define EV_LED 0x11
#define EV_SND 0x12
#define EV_REP 0x14
#define EV_FF 0x15
#define EV_PWR 0x16
#define EV_FF_STATUS 0x17
#define EV_MAX 0x1f
#define EV_CNT (EV_MAX + 1)


enum virtio_input_config_select {
  VIRTIO_INPUT_CFG_UNSET = 0x00,
  // subsel is zero. Returns the name of the device in u.string
  VIRTIO_INPUT_CFG_ID_NAME = 0x01,
  // subsel is zero. Returns the serial number of the device in u.string
  VIRTIO_INPUT_CFG_ID_SERIAL = 0x02,
  // subsel is zero. Returns the id information of the device in u.ids
  VIRTIO_INPUT_CFG_ID_DEVIDS = 0x03,
  // subsel is zero. Returns the input properties of the device in u.bitmap. Individual bits in the
  // bitmap correspond to INPUT_PROP_* constants
  VIRTIO_INPUT_CFG_PROP_BITS = 0x10,
  // subsel specifies the event type using EV_* constants. If size is non-zero, the event type is
  // supported and a bitmap of supported event codes is returned in u.bitmap.
  VIRTIO_INPUT_CFG_EV_BITS = 0x11,
  // subsel specifies the absolute axis using ABS_* constants in the underlying evdev
  // implementation. Information about the axis will be returned in u.abs
  VIRTIO_INPUT_CFG_ABS_INFO = 0x12,
};

struct virtio_input_absinfo {
  uint32_t min;
  uint32_t max;
  uint32_t fuzz;
  uint32_t flat;
  uint32_t res;
};

struct virtio_input_devids {
  uint16_t bustype;
  uint16_t vendor;
  uint16_t product;
  uint16_t version;
};

struct virtio_input_config {
  uint8_t select;
  uint8_t subsel;
  uint8_t size;
  uint8_t reserved[5];
  union {
    char string[128];
    uint8_t bitmap[128];
    struct virtio_input_absinfo abs;
    struct virtio_input_devids ids;
  } u;
};



struct virtio_input_event {
  uint16_t type;
  uint16_t code;
  uint32_t value;
};



class virtio_mmio_input : public virtio_mmio_dev {
  struct virtio_input_event evt[64];

 public:
  virtio_mmio_input(volatile uint32_t *regs);
  virtual ~virtio_mmio_input(void);

  virtual bool initialize(const struct virtio_config &config);
  virtual void irq(int ring_index, virtio::virtq_used_elem *);

  inline auto &config(void) {
    return *(virtio_input_config *)((off_t)this->regs + 0x100);
  }

	uint8_t cfg_select(uint8_t select, uint8_t subsel);
};
