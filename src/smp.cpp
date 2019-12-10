#include <func.h>
#include <mem.h>
#include <paging.h>
#include <smp.h>
#include <vec.h>

#define BASE_MEM_LAST_KILO 0x9fc00
#define BIOS_ROM_START 0xf0000
#define BIOS_ROM_END 0xfffff

// #define SMP_DEBUG

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

static uint32_t *lapic = NULL;

void smp::lapic_init(void) {
  if (!lapic) return;
  // Enable local APIC; set spurious interrupt vector.
  lapic_write(LAPIC_SVR, LAPIC_ENABLE | (32 + 31 /* spurious */));

  // The timer repeatedly counts down at bus frequency
  // from lapic[TICR] and then issues an interrupt.
  // If xv6 cared more about precise timekeeping,
  // TICR would be calibrated using an external time source.
  lapic_write(LAPIC_TDCR, LAPIC_X1);
  lapic_write(LAPIC_TIMER, LAPIC_PERIODIC | (32));
  lapic_write(LAPIC_TICR, 1000000);

  // Disable logical interrupt lines.
  lapic_write(LAPIC_LINT0, LAPIC_MASKED);
  lapic_write(LAPIC_LINT1, LAPIC_MASKED);

  // Disable performance counter overflow interrupts
  // on machines that provide that interrupt entry.
  if (((lapic[LAPIC_VER] >> 16) & 0xFF) >= 4) {
    lapic_write(LAPIC_PCINT, LAPIC_MASKED);
  }

  // Map error interrupt to IRQ_ERROR.
  lapic_write(LAPIC_ERROR, 32 + 19);

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
}

void smp::lapic_write(int ind, int value) {
  lapic[ind] = value;
  (void)lapic[ind];  // wait for write to finish, by reading
}

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
    if (n++ == 0)
      printk("cpu called from %p with interrupts enabled\n",
             __builtin_return_address(0));
  }

  if (!lapic) return 0;

  id = lapic[LAPIC_ID] >> 24;

  return id;
  /*
  for (n = 0; n < ncpu; n++)
    if (id == cpus[n].apicid)
      return n;

  return 0;
  */
}

void smp::lapic_eoi(void) {
  if (lapic) lapic_write(LAPIC_EOI, 0);
}

smp::mp::mp_table_entry_ioapic *ioapic_entry = NULL;

static smp::mp::mp_float_ptr_struct *find_mp_floating_ptr(void) {
  auto *cursor = (u32 *)p2v(BASE_MEM_LAST_KILO);

  while ((u64)cursor < (u64)p2v(BASE_MEM_LAST_KILO) + PAGE_SIZE) {
    if (*cursor == smp::mp::flt_signature) {
      return (smp::mp::mp_float_ptr_struct *)cursor;
    }
    cursor++;
  }

  cursor = (u32 *)p2v(BIOS_ROM_START);

  while ((u64)cursor < (u64)p2v(BIOS_ROM_END)) {
    if (*cursor == smp::mp::flt_signature) {
      return (smp::mp::mp_float_ptr_struct *)cursor;
    }
    cursor++;
  }

  return nullptr;
}

static u8 mp_entry_lengths[5] = {
    MP_TAB_CPU_LEN,    MP_TAB_BUS_LEN,  MP_TAB_IOAPIC_LEN,
    MP_TAB_IO_INT_LEN, MP_TAB_LINT_LEN,
};

void parse_mp_cpu(smp::mp::mp_table_entry_cpu *ent) {
  INFO("CPU: %p\n", ent);


  INFO("type: %02x\n", ent->type);
  INFO("lapic_id: %02x\n", ent->lapic_id);
  INFO("lapic_version: %02x\n", ent->lapic_version);
  INFO("enabled: %d\n", ent->enabled);
  INFO("is_bsp: %d\n", ent->is_bsp);
  INFO("sig: %08x\n", ent->sig);
  INFO("features: %08x\n", ent->feat_flags);
  INFO("\n");
}

void parse_mp_bus(smp::mp::mp_table_entry_bus *ent) {
  INFO("BUS: %p\n", ent);
  INFO("BUS: '%s'\n", ent->bus_type_string);
}

void parse_mp_ioapic(smp::mp::mp_table_entry_ioapic *ent) {
  INFO("IOAPIC: %p\n", ent);
  printk("%p\n", ent->addr);
}

void parse_mp_ioint(smp::mp::mp_table_entry_ioint *ent) {
  KINFO("IOINT: %p\n", ent);
  KINFO("type=%d\n", ent->int_type);
  KINFO("ioapic_id=%d\n", ent->dst_ioapic_id);
  KINFO("ioapic_intin=%d\n", ent->dst_ioapic_intin);
}

void parse_mp_lint(smp::mp::mp_table_entry_lint *ent) {
  // INFO("LINT: %p\n", ent);
}

static void walk_mp_table(smp::mp::mp_table *table, func<void(u8, void *)> fn) {
  auto count = table->entry_cnt;

  u8 *mp_entry;

  mp_entry = (u8 *)&table->entries;
  // TODO: check table integrity
  while (--count) {
    auto type = *mp_entry;
    fn(type, mp_entry);
    mp_entry += mp_entry_lengths[type];
  }
}

static bool parse_mp_table(smp::mp::mp_table *table) {
  lapic = (uint32_t *)p2v((uint64_t)table->lapic_addr);

  printk("lapic=%p\n", lapic);

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

// global variable that stores the CPUs
static vec<smp::cpu_state *> cpus;

smp::cpu_state &smp::get_state(void) {
  // TODO: get the real cpu number
  auto cpu_index = 0;

  // return the cpu at that index, unchecked.
  return *cpus[cpu_index];
}

bool smp::init(void) {
  mp_floating_ptr = find_mp_floating_ptr();
  if (mp_floating_ptr == nullptr) return false;

  // we found the mp floating table, now to parse it...
  INFO("mp_floating_table @ %p\n", mp_floating_ptr);

  u64 table_addr = mp_floating_ptr->mp_cfg_ptr;

  if (!parse_mp_table((mp::mp_table *)p2v(table_addr))) {
    return false;
  }

  printk("cpunum = %d\n", cpunum());

  // mp table was parsed and loaded into global memory
  INFO("ncpus: %d\n", cpus.size());
  return true;
}

void smp::init_cores(void) {}
