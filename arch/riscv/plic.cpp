#include <riscv/plic.h>
#include <riscv/arch.h>
#include <riscv/memlayout.h>
#include <printf.h>
#include <util.h>
#include <cpu.h>

#include <dev/hardware.h>
#include <dev/driver.h>
int boot_hart = -1;


#define LOG(...) PFXLOG(BLU "riscv,plic", __VA_ARGS__)


// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC ((rv::xsize_t)p2v(this->base))
#define PLIC_PRIORITY MREG(PLIC + 0x0)
#define PLIC_PENDING MREG(PLIC + 0x1000)
#define PLIC_MENABLE(hart) MREG(PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) MREG(PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) MREG(PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) MREG(PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) MREG(PLI + 0x201004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) MREG(PLIC + 0x200004 + (hart)*0x2000)

/*
 * Each interrupt source has a priority register associated with it.
 * We always hardwire it to one in Linux.
 */
#define PRIORITY_BASE 0

/*
 * Each hart context has a vector of interrupt enable bits associated with it.
 * There's one bit for each interrupt source.
 */
#define ENABLE_BASE 0x2000
#define ENABLE_PER_HART (1024 / 4)




class RISCVPlic : public dev::CharDevice {
 public:
  using dev::CharDevice::CharDevice;

  virtual ~RISCVPlic(void) {}

  virtual void init(void);
  int claim();
  int pending();
  void complete(int irq);
  void toggle(int hart, int hwirq, int priority, bool enable);

  // initialize a hart after it boots
  void hart_init();
  off_t base = 0;
  spinlock lock;
  int ndev = 0;

  void set_priority(int irq, int prio);

  inline void write_reg(off_t off, uint32_t value) { *(volatile uint32_t *)(base + off) = value; }
  inline uint32_t read_reg(off_t off) { return *(volatile uint32_t *)(base + off); }
};


static RISCVPlic *the_plic = nullptr;
void RISCVPlic::init(void) {
  scoped_irqlock l(lock);
  the_plic = this;

  auto mmio = dev()->cast<hw::MMIODevice>();
  base = mmio->address();

  ndev = mmio->get_prop_int("riscv,ndev").or_default(0);
  LOG("found a plic at %p with %d devices\n", base, ndev);
  for (int irq = 1; irq < ndev; irq++) {
    // mask every device
    set_priority(irq, 0);
  }
}

void RISCVPlic::set_priority(int irq, int prio) {
	write_reg(4 * irq + PRIORITY_BASE, prio);
}


void RISCVPlic::hart_init(void) {
	scoped_irqlock l(lock);

	//
}

int RISCVPlic::pending(void) {
  scoped_irqlock l(lock);
  return PLIC_PENDING;
}

int RISCVPlic::claim(void) {
  scoped_irqlock l(lock);
  int hart = rv::hartid();
  int irq = PLIC_SCLAIM(hart);
  return irq;
}

void RISCVPlic::complete(int irq) {
  scoped_irqlock l(lock);
  int hart = rv::hartid();
  PLIC_SCLAIM(hart) = irq;
}



void RISCVPlic::toggle(int hart, int hwirq, int priority, bool enable) {
  off_t enable_base = PLIC + ENABLE_BASE + hart * ENABLE_PER_HART;
  volatile uint32_t &reg = MREG(enable_base + (hwirq / 32) * 4);
  uint32_t hwirq_mask = 1 << (hwirq % 32);
  // MREG(PLIC + 4 * hwirq) = 7;
  // PLIC_SPRIORITY(hart) = 0;
	set_priority(hwirq, priority);

  if (enable) {
    reg = reg | hwirq_mask;
  } else {
    reg = reg & ~hwirq_mask;
  }

}


static dev::ProbeResult riscv_plic_probe(ck::ref<hw::Device> dev) {
  if (auto mmio = dev->cast<hw::MMIODevice>()) {
    if (mmio->is_compat("riscv,plic0")) {
      return dev::ProbeResult::Attach;
    }
  }

  return dev::ProbeResult::Ignore;
};


void rv::plic::discover(void) {
  // add a driver which can discover any plics on the system. No clue if we
  // have to handle multiple yet, as there aren't all that many riscv boards
  // around, but everything I've used
  dev::Driver::add(ck::make_ref<dev::ModuleDriver<RISCVPlic>>("riscv,plic0", riscv_plic_probe));
}
void rv::plic::hart_init(void) {
  assert(the_plic != NULL);

  the_plic->hart_init();
  /*
int hart = rv::hartid();
LOG("Initializing on hart#%d\n", hart);

for (int i = 0; i < 0x1000 / 4; i++)
MREG(PLIC + i * 4) = 7;

(&PLIC_SENABLE(hart))[0] = 0;
(&PLIC_SENABLE(hart))[1] = 0;
(&PLIC_SENABLE(hart))[2] = 0;
  */
}

uint32_t rv::plic::pending(void) {
  assert(the_plic != NULL);
  return the_plic->pending();
}


int rv::plic::claim(void) {
  assert(the_plic != NULL);
  return the_plic->claim();
}

void rv::plic::complete(int irq) {
  assert(the_plic != NULL);
  return the_plic->complete(irq);
}


void rv::plic::enable(int hwirq, int priority) {
  assert(the_plic != NULL);
  LOG("enable hwirq=%d, priority=%d\n", hwirq, priority);
  the_plic->toggle(rv::hartid(), hwirq, priority, true);
}

void rv::plic::disable(int hwirq) {
  assert(the_plic != NULL);
  the_plic->toggle(rv::hartid(), hwirq, 7, false);
}
