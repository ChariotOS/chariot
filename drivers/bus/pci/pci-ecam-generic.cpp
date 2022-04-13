#include <dev/hardware.h>
#include <dev/driver.h>
#include <module.h>
#include <util.h>

#define COMPAT "pci-host-ecam-generic"
#define LOG(...) PFXLOG(BLU COMPAT, __VA_ARGS__)


class EcamPCIBus : public dev::Device {
 public:
  using dev::Device::Device;

	void init(void) override {
		LOG("override\n");
	}
};


driver_init(COMPAT, EcamPCIBus, [](ck::ref<hw::Device> dev) -> dev::ProbeResult {
  if (auto mmio = dev->cast<hw::MMIODevice>()) {
    if (mmio->is_compat(COMPAT)) {
      LOG("Found device @%08llx. irq=%d\n", mmio->address(), mmio->interrupt);
      return dev::ProbeResult::Attach;
    }
  }
  return dev::ProbeResult::Ignore;
});
/*
class FU740PciDriver : public dev::Module {
 public:
  virtual ~FU740PciDriver(void) {}

  dev::ProbeResult probe(ck::ref<hw::Device> dev) override {
    if (auto mmio = dev->cast<hw::MMIODevice>()) {
      if (mmio->is_compat("sifive,fu740-pcie")) {
        LOG("Found device @%08llx\n", mmio->address());
                                // hexdump(p2v(mmio->address()), 4096, true);
      }
    }
    return dev::ProbeResult::Ignore;
  };
};
*/

// driver_init(COMPAT, FU740PciDriver);
