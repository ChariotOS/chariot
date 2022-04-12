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


#define PLIC_MAX_IRQS 1024

#define PLIC_PRIORITY_BASE 0x000000U

#define PLIC_ENABLE_BASE 0x002000U
#define PLIC_ENABLE_STRIDE 0x80U
#define IRQ_ENABLE 1
#define IRQ_DISABLE 0

#define PLIC_CONTEXT_BASE 0x200000U
#define PLIC_CONTEXT_STRIDE 0x1000U
#define PLIC_CONTEXT_THRESHOLD 0x0U
#define PLIC_CONTEXT_CLAIM 0x4U



#define PLIC_PRIORITY(n) (PLIC_PRIORITY_BASE + (n) * sizeof(uint32_t))
#define PLIC_ENABLE(n, h) (contexts[h].enable_offset + ((n) / 32) * sizeof(uint32_t))
#define PLIC_THRESHOLD(h) (contexts[h].context_offset + PLIC_CONTEXT_THRESHOLD)
#define PLIC_CLAIM(h) (contexts[h].context_offset + PLIC_CONTEXT_CLAIM)



struct plic_context {
  off_t enable_offset;
  off_t context_offset;
};

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

  void set_priority(int irq, int prio);
  void set_threshold(int cpu, uint32_t threshold);

  inline void write_reg(off_t off, uint32_t value) { *(volatile uint32_t *)(base + off) = value; }
  inline uint32_t read_reg(off_t off) { return *(volatile uint32_t *)(base + off); }

 private:
  off_t base = 0;
  spinlock lock;
  int ndev = 0;
  plic_context contexts[CONFIG_MAX_CPUS];
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
    set_priority(irq, 7);
  }

  ck::vec<uint64_t> ints_extended;
  if (mmio->props().contains("interrupts-extended")) {
    auto &prop = mmio->props().get("interrupts-extended");
    prop.read_all_ints(ints_extended, "i");
  }

  int context, i;


  uint64_t *cells = ints_extended.data();
  for (i = 0, context = 0; i < ints_extended.size(); i += 2, context++) {
    // if this won't deliver irq nr 9 to the cpu core, skip it. This deals
    // with the strange 0xFFFFFFFF values here for M-Mode interrupts
    if (cells[i + 1] != 9) continue;


    // search for interrupt controller that this context targets. If we find
    // one, we can grab it's parent to get the core id that controls this
    // particular context.
    auto intc = hw::find_phandle(cells[i]);
    if (intc) {
      auto core = intc->parent();
      assert(core != nullptr);
      if (auto mmio = core->cast<hw::MMIODevice>()) {
        auto hartid = mmio->address();


        contexts[hartid].enable_offset = PLIC_ENABLE_BASE + context * PLIC_ENABLE_STRIDE;
        contexts[hartid].context_offset = PLIC_CONTEXT_BASE + context * PLIC_CONTEXT_STRIDE;

        LOG("CPU %d has context %d\n", hartid, context);
      }
    }

    // printf("cpu %p at phandle %d has %d\n", dev.get(), cells[i], cells[i + 1]);
  }
}


void RISCVPlic::set_threshold(int cpu, uint32_t threshold) {
  uint32_t prival;

  if (threshold < 4)  // enable everything (as far as plic is concerned)
    prival = 0;
  else if (threshold >= 12)  // invalid priority level ?
    prival = 12 - 4;         // XXX Device-specific high threshold
  else                       // everything else
    prival = threshold - 4;  // XXX Device-specific threshold offset

  write_reg(PLIC_THRESHOLD(cpu), prival);
}

void RISCVPlic::set_priority(int irq, int pri) {
  uint32_t prival;

  /*
   * sifive plic only has 0 - 7 priority levels, yet OpenBSD defines
   * 0 - 12 priority levels(level 1 - 4 are for SOFT*, level 12
   * is for IPI. They should NEVER be passed to plic.
   * So we calculate plic priority in the following way:
   */
  if (pri <= 4 || pri >= 12)  // invalid input
    prival = 0;               // effectively disable this intr source
  else
    prival = pri - 4;

  write_reg(PLIC_PRIORITY(irq), prival);
}


void RISCVPlic::hart_init(void) {
  scoped_irqlock l(lock);

  // set the thresh to the minimum
  set_threshold(rv::hartid(), 0);
}

int RISCVPlic::pending(void) {
  scoped_irqlock l(lock);
  return 0;
}

int RISCVPlic::claim(void) {
  scoped_irqlock l(lock);
  int hart = rv::hartid();
  int irq = read_reg(PLIC_CLAIM(hart));
  return irq;
}

void RISCVPlic::complete(int irq) {
  scoped_irqlock l(lock);
  int hart = rv::hartid();
  write_reg(PLIC_CLAIM(hart), irq);
}



void RISCVPlic::toggle(int hart, int hwirq, int priority, bool enable) {
  printf("toggle %d to %d\n", hwirq, enable);
  if (hwirq == 0) return;
  assert(hart < CONFIG_MAX_CPUS);

  uint32_t mask = (1 << (hwirq % 32));
  uint32_t val = read_reg(PLIC_ENABLE(hwirq, hart));
  if (enable == IRQ_ENABLE)
    val |= mask;
  else
    val &= ~mask;

  write_reg(PLIC_ENABLE(hwirq, hart), val);
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
