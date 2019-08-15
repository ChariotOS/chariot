#include <module.h>
#include <pci.h>
#include <printk.h>
#include <vga.h>

vga::vesa::vesa() { m_framebuffer_address = find_framebuffer_address(); }

u64 vga::vesa::find_framebuffer_address() {
  u64 fba = 0;
  pci::walk_devices([&](pci::device *dev) {
    // virtualbox vga
    if (dev->is_device(0x80ee, 0xbeef) || dev->is_device(0x1234, 0x1111)) {
      fba = (u64)dev->get_bar(0).addr;
    }
  });

  m_framebuffer_address = fba;

  return 0;
}

static void init(void) { vga::vesa v; }

module_init("VESA", init);
