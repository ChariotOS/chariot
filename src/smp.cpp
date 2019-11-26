#include <mem.h>
#include <paging.h>
#include <smp.h>
#include <vec.h>

#define BASE_MEM_LAST_KILO 0x9fc00
#define BIOS_ROM_START 0xf0000
#define BIOS_ROM_END 0xfffff

// #define SMP_DEBUG

#ifdef SMP_DEBUG
#define INFO(fmt, args...) KINFO("SMP: " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

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
  // INFO("BUS: %p\n", ent);
}

void parse_mp_ioapic(smp::mp::mp_table_entry_ioapic *ent) {
  // INFO("IOAPIC: %p\n", ent);
}

void parse_mp_ioint(smp::mp::mp_table_entry_ioint *ent) {
  // INFO("IOINT: %p\n", ent);
}

void parse_mp_lint(smp::mp::mp_table_entry_lint *ent) {
  // INFO("LINT: %p\n", ent);
}

static bool parse_mp_table(smp::mp::mp_table *table) {
  auto count = table->entry_cnt;

  u8 *mp_entry;

  mp_entry = (u8 *)&table->entries;

  // TODO: check table integrity
  while (--count) {
    auto type = *mp_entry;

    switch (type) {
      case MP_TAB_TYPE_CPU:
        parse_mp_cpu((smp::mp::mp_table_entry_cpu *)mp_entry);
        break;

      case MP_TAB_TYPE_BUS:
        parse_mp_bus((smp::mp::mp_table_entry_bus *)mp_entry);
        break;

      case MP_TAB_TYPE_IOAPIC:
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
        return false;
    }

    mp_entry += mp_entry_lengths[type];
  }
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

  // mp table was parsed and loaded into global memory
  INFO("ncpus: %d\n", cpus.size());
  return true;
}

