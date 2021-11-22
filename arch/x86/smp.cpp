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
#include <module.h>
#include <sched.h>
#include <x86/fpu.h>
#include <x86/msr.h>

#define IPI_IRQ (0xF3 - 32)


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

static volatile uint32_t *global_lapic = NULL;





/**
 * Converts an entry in a local APIC's Local Vector Table to a
 * human-readable string.
 * (NOTE: taken from Kitten)
 */
static void apic_print_lvt(uint32_t entry) {
  uint32_t delivery_mode = entry & 0x700;

  if (delivery_mode == APIC_DEL_MODE_FIXED) {
    printk("FIXED -> IDT VECTOR %u", entry & APIC_LVT_VEC_MASK);
  } else if (delivery_mode == APIC_DEL_MODE_NMI) {
    printk("NMI   -> IDT VECTOR 2");
  } else if (delivery_mode == APIC_DEL_MODE_EXTINT) {
    printk("ExtINT, hooked to old 8259A PIC");
  } else {
    printk("UNKNOWN");
  }

  if (entry & APIC_LVT_DISABLED) printk(", MASKED");
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

#if 1
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
  global_lapic = (uint32_t *)p2v((uint64_t)table->lapic_addr);

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
  return true;
}

ksh_def("smp-scan", "scan the smp tables") {
  smp::init();
  return 0;
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
  // Send INIT (level-triggered) interrupt to reset other CPU
	core().apic.write_icr(id, ICR_DEL_MODE_INIT | ICR_TRIG_MODE_LEVEL | ICR_LEVEL_ASSERT);
  microdelay(200);
	// de-assert
	core().apic.write_icr(id, ICR_DEL_MODE_INIT | ICR_TRIG_MODE_LEVEL);
  microdelay(100);  // should be 10ms, but too slow in Bochs!

  // Send startup IPI (twice!) to enter code.
  // Regular hardware is supposed to only accept a STARTUP
  // when it is in the halted state due to an INIT.  So the second
  // should be ignored, but it is part of the official Intel algorithm.
  // Bochs complains about the second one.  Too bad for Bochs.
  for (i = 0; i < 2; i++) {
		core().apic.write_icr(id, ICR_DEL_MODE_STARTUP | (code >> 12));
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


	struct processor_state cpu;
  // initialize the CPU
  cpu::seginit(&cpu, NULL);
  cpu::current().id = apic_id;

  // load the IDT
  lidt((uint32_t *)&idt_block, 4096);
  fpu::init();


	// initialize our apic
	core().apic.init();

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
    if (core.entry->lapic_id == core_id()) continue;

    args->stack = (unsigned long)p2v(phys::alloc()) + 4096;
    args->ready = 0;
    args->apic_id = core.entry->lapic_id;

    startap(core.entry->lapic_id, (unsigned long)v2p(code));
    while (args->ready == 0) {
      arch_relax();
    }
  }

  arch_enable_ints();
}



ksh_def("smp-start", "start the other cores on the system") {
  smp::init_cores();
  return 0;
}

