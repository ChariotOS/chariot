#include <x86/smp.h>

#include <cpu.h>
#include <ck/func.h>
#include <idt.h>
#include <mem.h>
#include <paging.h>
#include <phys.h>
#include <pit.h>
#include <time.h>
#include <util.h>
#include <ck/vec.h>
#include <sched.h>
#include <x86/fpu.h>


extern "C" void enable_sse();
extern "C" int gdtr[];
extern "C" int gdtr32[];
extern "C" char smp_trampoline_start[];
extern "C" uint64_t boot_p4[];
extern uint64_t *kernel_page_table;
extern uint32_t idt_block[];

#define BASE_MEM_LAST_KILO 0x9fc00
#define BIOS_ROM_START 0xf0000
#define BIOS_ROM_END 0xfffff

#define SMP_DEBUG

#ifdef SMP_DEBUG
#define INFO(fmt, args...) KINFO("[SMP] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

// Local APIC registers, divided by 4 for use as uint[] indices.
#define LAPIC_ID (0x0020 / 4)     // ID
#define LAPIC_VER (0x0030 / 4)    // Version
#define LAPIC_TPR (0x0080 / 4)    // Task Priority
#define LAPIC_EOI (0x00B0 / 4)    // EOI
#define LAPIC_SVR (0x00F0 / 4)    // Spurious Interrupt Vector
#define LAPIC_ENABLE 0x00000100   // Unit Enable
#define LAPIC_ESR (0x0280 / 4)    // Error Status
#define LAPIC_ICRLO (0x0300 / 4)  // Interrupt Command
#define LAPIC_INIT 0x00000500     // INIT/RESET
#define LAPIC_STARTUP 0x00000600  // Startup IPI
#define LAPIC_DELIVS 0x00001000   // Delivery status
#define LAPIC_ASSERT 0x00004000   // Assert interrupt (vs deassert)
#define LAPIC_DEASSERT 0x00000000
#define LAPIC_LEVEL 0x00008000  // Level triggered
#define LAPIC_BCAST 0x00080000  // Send to all APICs, including self.
#define LAPIC_BUSY 0x00001000
#define LAPIC_FIXED 0x00000000
#define LAPIC_ICRHI (0x0310 / 4)   // Interrupt Command [63:32]
#define LAPIC_TIMER (0x0320 / 4)   // Local Vector Table 0 (TIMER)
#define LAPIC_X1 0x0000000B        // divide counts by 1
#define LAPIC_PERIODIC 0x00020000  // Periodic
#define LAPIC_PCINT (0x0340 / 4)   // Performance Counter LVT
#define LAPIC_LINT0 (0x0350 / 4)   // Local Vector Table 1 (LINT0)
#define LAPIC_LINT1 (0x0360 / 4)   // Local Vector Table 2 (LINT1)
#define LAPIC_ERROR (0x0370 / 4)   // Local Vector Table 3 (ERROR)
#define LAPIC_MASKED 0x00010000    // Interrupt masked
#define LAPIC_TICR (0x0380 / 4)    // Timer Initial Count
#define LAPIC_TCCR (0x0390 / 4)    // Timer Current Count
#define LAPIC_TDCR (0x03E0 / 4)    // Timer Divide Configuration

// ioapic is always at the same location
volatile auto *ioapic = (volatile struct ioapic *)p2v(0xFEC00000);

// IO APIC MMIO structure: write reg, then read or write data.
struct ioapic {
  uint32_t reg;
  uint32_t pad[3];
  uint32_t data;
};

#define REG_ID 0x00     // Register index: ID
#define REG_VER 0x01    // Register index: version
#define REG_TABLE 0x10  // Redirection table base

static void ioapicwrite(int reg, uint32_t data) {
  ioapic->reg = reg;
  ioapic->data = data;
}

void smp::ioapicenable(int irq, int cpunum) {
  if (irq == 0) return;
  // Mark interrupt edge-triggered, active high,
  // enabled, and routed to the given cpunum,
  // which happens to be that cpu's APIC ID.
  ioapicwrite(REG_TABLE + 2 * irq, T_IRQ0 + irq);
  ioapicwrite(REG_TABLE + 2 * irq + 1, cpunum << 24);
}

static volatile uint32_t *lapic = NULL;
// how fast does the lapic tick in the time of a kernel tick?
static uint32_t lapic_ticks_per_second = 0;

static void wait_for_tick_change(void) {
  volatile uint64_t start = cpu::get_ticks();
  while (cpu::get_ticks() == start) {
  }
}

static void lapic_tick_handler(int i, reg_t *tf, void *) {
  auto &cpu = cpu::current();
  uint64_t now = arch_read_timestamp();
  cpu.kstat.tsc_per_tick = now - cpu.kstat.last_tick_tsc;
  cpu.kstat.last_tick_tsc = now;
  cpu.kstat.ticks++;


  smp::lapic_eoi();
  sched::handle_tick(cpu.kstat.ticks);
}



// will screw up the PIT
static void calibrate(void) {
#define PIT_DIV 100
#define DATASET_SIZE 10

  set_pit_freq(PIT_DIV);



  uint64_t total_ticks = 0;
  for (int i = 1; i < DATASET_SIZE; i++) {
    wait_for_tick_change();

    // Set APIC init counter to -1
    smp::lapic_write(LAPIC_TICR, 0xffffffff);

    wait_for_tick_change();
    // Stop the APIC timer
    smp::lapic_write(LAPIC_TIMER, LAPIC_MASKED);

    // Now we know how often the APIC timer has ticked in 10ms
    auto ticks = 0xffffffff - smp::lapic_read(LAPIC_TCCR);
    // printk("%d ticks\n", ticks);
    total_ticks += ticks;
  }

  auto avg = (total_ticks / DATASET_SIZE);
  printk(KERN_INFO "%d avg ticks\n", avg);


  lapic_ticks_per_second = avg * PIT_DIV;

  printk(KERN_INFO "%d ticks per second\n", lapic_ticks_per_second);
}

static void set_tickrate(uint32_t per_second) {
  cpu::current().ticks_per_second = per_second;
  smp::lapic_write(LAPIC_TICR,
      lapic_ticks_per_second / per_second);  // set the tick rate
}

void smp::lapic_init(void) {
  if (!lapic) return;

  struct cpuid_busfreq_info freq;

  cpuid_busfreq(&freq);


  lapic_write(LAPIC_TDCR, LAPIC_X1);

  if (lapic_ticks_per_second == 0) {
    KINFO("[LAPIC] freq info: base: %uMHz,  max: %uMHz, bus: %uMHz\n", freq.base, freq.max, freq.bus);
    calibrate();
    KINFO("[LAPIC] counts per tick: %zu\t0x%08x\n", lapic_ticks_per_second, lapic_ticks_per_second);
  }


  // Enable local APIC; set spurious interrupt vector.
  lapic_write(LAPIC_SVR, LAPIC_ENABLE | (31 /* spurious */));

  // set the periodic interrupt timer to be IRQ 50
  // This is so we can use the PIT for sleep related activities at IRQ 32
  smp::lapic_write(LAPIC_TDCR, LAPIC_X1);
  smp::lapic_write(LAPIC_TIMER, LAPIC_PERIODIC | (50 + 32));
  set_tickrate(CONFIG_TICKS_PER_SECOND);  // tick every ms

  // Disable logical interrupt lines.
  lapic_write(LAPIC_LINT0, LAPIC_MASKED);
  lapic_write(LAPIC_LINT1, LAPIC_MASKED);

  // Disable performance counter overflow interrupts
  // on machines that provide that interrupt entry.
  if (((lapic[LAPIC_VER] >> 16) & 0xFF) >= 4) {
    lapic_write(LAPIC_PCINT, LAPIC_MASKED);
  }

  // Map error interrupt to IRQ_ERROR.
  lapic_write(LAPIC_ERROR, IRQ_ERROR);

  // Clear error status register (requires back-to-back writes).
  lapic_write(LAPIC_ESR, 0);
  lapic_write(LAPIC_ESR, 0);

  // Ack any outstanding interrupts.
  lapic_write(LAPIC_EOI, 0);

  // Send an Init Level De-Assert to synchronise arbitration ID's.
  lapic_write(LAPIC_ICRHI, 0);
  lapic_write(LAPIC_ICRLO, LAPIC_BCAST | LAPIC_INIT | LAPIC_LEVEL);
  while (lapic[LAPIC_ICRLO] & LAPIC_DELIVS)
    ;

  // Enable interrupts on the APIC (but not on the processor).
  lapic_write(LAPIC_TPR, 0);

  irq::uninstall(0);
  irq::install(50, lapic_tick_handler, "Local APIC Preemption Tick");
}

void smp::lapic_write(int ind, int value) {
  lapic[ind] = value;
  (void)lapic[ind];  // wait for write to finish, by reading
}
unsigned smp::lapic_read(int ind) { return lapic[ind]; }

int smp::cpunum(void) {
  // int n = 0;
  int id = 0;
  // Cannot call cpu when interrupts are enabled:
  // result not guaranteed to last long enough to be used!
  // Would prefer to panic but even printing is chancy here:
  // almost everything, including cprintf and panic, calls cpu,
  // often indirectly through acquire and release.
  if (readeflags() & FL_IF) {
    static int n;
    if (n++ == 0) printk("cpu called from %p with interrupts enabled\n", __builtin_return_address(0));
  }

  if (!lapic) return 0;

  id = lapic[LAPIC_ID] >> 24;

  return id;
}

void smp::lapic_eoi(void) {
  if (lapic) lapic_write(LAPIC_EOI, 0);
}

smp::mp::mp_table_entry_ioapic *ioapic_entry = NULL;

static smp::mp::mp_float_ptr_struct *find_mp_floating_ptr(void) {
  auto *cursor = (uint32_t *)p2v(BASE_MEM_LAST_KILO);

  while ((uint64_t)cursor < (uint64_t)p2v(BASE_MEM_LAST_KILO) + PAGE_SIZE) {
    if (*cursor == smp::mp::flt_signature) {
      return (smp::mp::mp_float_ptr_struct *)cursor;
    }
    cursor++;
  }

  cursor = (uint32_t *)p2v(BIOS_ROM_START);

  while ((uint64_t)cursor < (uint64_t)p2v(BIOS_ROM_END)) {
    if (*cursor == smp::mp::flt_signature) {
      return (smp::mp::mp_float_ptr_struct *)cursor;
    }
    cursor++;
  }

  return nullptr;
}

// global variable that stores the CPUs
static smp::cpu_state apic_cpus[CONFIG_MAX_CPUS];
static int ncpus = 0;

static u8 mp_entry_lengths[5] = {
    MP_TAB_CPU_LEN,
    MP_TAB_BUS_LEN,
    MP_TAB_IOAPIC_LEN,
    MP_TAB_IO_INT_LEN,
    MP_TAB_LINT_LEN,
};

void parse_mp_cpu(smp::mp::mp_table_entry_cpu *ent) {
  // Allocate a new smp::cpu_state and insert it into the cpus vec
  auto state = smp::cpu_state{.entry = ent};
  state.index = ent->lapic_id;
  apic_cpus[ent->lapic_id] = state;
  ncpus++;

#if 0
	INFO("CPU: %p\n", ent);
	INFO("type: %02x\n", ent->type);
	INFO("lapic_id: %02x\n", ent->lapic_id);
	INFO("lapic_version: %02x\n", ent->lapic_version);
	INFO("enabled: %d\n", ent->enabled);
	INFO("is_bsp: %d\n", ent->is_bsp);
	INFO("sig: %08x\n", ent->sig);
	INFO("features: %08x\n", ent->feat_flags);
	INFO("\n");
#endif
}

void parse_mp_bus(smp::mp::mp_table_entry_bus *ent) {}
void parse_mp_ioapic(smp::mp::mp_table_entry_ioapic *ent) {}
void parse_mp_ioint(smp::mp::mp_table_entry_ioint *ent) {}
void parse_mp_lint(smp::mp::mp_table_entry_lint *ent) {}

static void walk_mp_table(smp::mp::mp_table *table, ck::func<void(u8, void *)> fn) {
  auto count = table->entry_cnt;
  u8 *mp_entry;
  mp_entry = (u8 *)&table->entries;
  while (--count) {
    auto type = *mp_entry;
    fn(type, mp_entry);
    mp_entry += mp_entry_lengths[type];
  }
}

static bool parse_mp_table(smp::mp::mp_table *table) {
  lapic = (uint32_t *)p2v((uint64_t)table->lapic_addr);

  walk_mp_table(table, [&](u8 type, void *mp_entry) {
    switch (type) {
      case MP_TAB_TYPE_CPU:
        parse_mp_cpu((smp::mp::mp_table_entry_cpu *)mp_entry);
        break;

      case MP_TAB_TYPE_BUS:
        parse_mp_bus((smp::mp::mp_table_entry_bus *)mp_entry);
        break;

      case MP_TAB_TYPE_IOAPIC:
        ioapic_entry = (smp::mp::mp_table_entry_ioapic *)mp_entry;
        parse_mp_ioapic((smp::mp::mp_table_entry_ioapic *)mp_entry);
        break;

      case MP_TAB_TYPE_IO_INT:
        parse_mp_ioint((smp::mp::mp_table_entry_ioint *)mp_entry);
        break;

      case MP_TAB_TYPE_LINT:
        parse_mp_lint((smp::mp::mp_table_entry_lint *)mp_entry);
        break;

      default:
        printk("unhandled mp_entry of type %02x\n", type);
    }
  });

  return true;
}

static smp::mp::mp_float_ptr_struct *mp_floating_ptr;

smp::cpu_state &smp::get_state(void) {
  // TODO: get the real cpu number
  auto cpu_index = 0;

  // return the cpu at that index, unchecked.
  return apic_cpus[cpu_index];
}

bool smp::init(void) {
#ifdef CONFIG_SMP
  mp_floating_ptr = find_mp_floating_ptr();
  if (mp_floating_ptr == nullptr) {
    debug("MP floating pointer not found!\n");
    return false;
  }

  // we found the mp floating table, now to parse it...
  INFO("mp_floating_table @ %p\n", mp_floating_ptr);

  uint64_t table_addr = mp_floating_ptr->mp_cfg_ptr;

  if (!parse_mp_table((mp::mp_table *)p2v(table_addr))) {
    debug("Unable to parse MP table\n");
    return false;
  }
#endif
  return true;
}

static inline void io_delay(void) {
  const uint16_t DELAY_PORT = 0x80;
  asm volatile("outb %%al,%0" : : "dN"(DELAY_PORT));
}

static void microdelay(int n) {
  while (n--) {
    io_delay();
  }
}

#define CMOS_PORT 0x70
static void startap(int id, unsigned long code) {
  int i;
  unsigned short *wrv;

  // "The BSP must initialize CMOS shutdown code to 0AH
  // and the warm reset vector (DWORD based at 40:67) to point at
  // the AP startup code prior to the [universal startup algorithm]."
  outb(CMOS_PORT, 0xF);  // offset 0xF is shutdown code
  outb(CMOS_PORT + 1, 0x0A);
  wrv = (unsigned short *)p2v((0x40 << 4 | 0x67));  // Warm reset vector
  wrv[0] = 0;
  wrv[1] = code >> 4;

  // "Universal startup algorithm."
  // Send INIT (level-triggered) interrupt to reset other CPU.
  smp::lapic_write(LAPIC_ICRHI, id << 24);
  smp::lapic_write(LAPIC_ICRLO, LAPIC_INIT | LAPIC_LEVEL | LAPIC_ASSERT);
  microdelay(200);
  smp::lapic_write(LAPIC_ICRLO, LAPIC_INIT | LAPIC_LEVEL);
  microdelay(100);  // should be 10ms, but too slow in Bochs!

  // Send startup IPI (twice!) to enter code.
  // Regular hardware is supposed to only accept a STARTUP
  // when it is in the halted state due to an INIT.  So the second
  // should be ignored, but it is part of the official Intel algorithm.
  // Bochs complains about the second one.  Too bad for Bochs.
  for (i = 0; i < 2; i++) {
    smp::lapic_write(LAPIC_ICRHI, id << 24);
    smp::lapic_write(LAPIC_ICRLO, LAPIC_STARTUP | (code >> 12));
    // TODO: on real hardware, we MUST actually wait here for 200us. But in qemu
    //       its fine because it is virtualized 'instantly'
    microdelay(200);
  }
}

// just a named tuple of 8byte values
struct ap_args {
  unsigned long ready;
  unsigned long stack;
  unsigned long apic_id;
  unsigned long gdtr32;
  unsigned long gdtr64;
  unsigned long boot_pt;
  unsigned long entry;
};


// Enters in 64bit mode
extern "C" void mpentry(int apic_id) {
  volatile auto args = (struct ap_args *)p2v(0x6000);

  // initialize the CPU
  cpu::seginit(NULL);
  cpu::current().cpunum = apic_id;

  // load the IDT
  lidt((uint32_t *)&idt_block, 4096);
  fpu::init();

  smp::lapic_init();

  // we're fully booted now
  args->ready = 1;

  KINFO("starting scheduler on core %d. tsc: %llu\n", apic_id, arch_read_timestamp());
  sched::init();
  arch_enable_ints();
  sched::run();

  while (1) {
    asm("hlt");
  }
  panic("mpentry should not have returned\n");
}


void smp::init_cores(void) {
#ifdef CONFIG_SMP

  arch_disable_ints();
  // copy the code into the AP region
  void *code = p2v(0x7000);
  auto sz = 4096;
  memcpy(code, p2v(smp_trampoline_start), sz);

  volatile auto args = (struct ap_args *)p2v(0x6000);

  args->gdtr32 = (unsigned long)gdtr32;
  args->gdtr64 = (unsigned long)gdtr;
  args->boot_pt = (unsigned long)v2p(kernel_page_table);
  for (int i = 0; i < ncpus; i++) {
    auto &core = apic_cpus[i];

    // skip ourselves
    if (core.entry->lapic_id == smp::cpunum()) continue;

    args->stack = (unsigned long)p2v(phys::alloc()) + 4096;
    args->ready = 0;
    args->apic_id = core.entry->lapic_id;

    startap(core.entry->lapic_id, (unsigned long)v2p(code));
    while (args->ready == 0) {
      arch_relax();
    }
  }

  arch_enable_ints();
#endif
}
