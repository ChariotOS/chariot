#pragma once

#include <asm.h>
#include <types.h>

#define MP_TAB_TYPE_CPU 0
#define MP_TAB_TYPE_BUS 1
#define MP_TAB_TYPE_IOAPIC 2
#define MP_TAB_TYPE_IO_INT 3
#define MP_TAB_TYPE_LINT 4

#define MP_TAB_CPU_LEN 20
#define MP_TAB_BUS_LEN 8
#define MP_TAB_IOAPIC_LEN 8
#define MP_TAB_IO_INT_LEN 8
#define MP_TAB_LINT_LEN 8


struct cpuid_busfreq_info {
  unsigned base, max, bus;
};

extern "C" {
// in cpuid.s
extern unsigned cpuid_busfreq(struct cpuid_busfreq_info *);
}

namespace smp {

  // the first field of the floating structure must be this value

  // use mp tables, as they are a little easier.
  namespace mp {

    constexpr u32 flt_signature = 0x5F504D5F;
    struct mp_float_ptr_struct {
      uint32_t sig;
      uint32_t mp_cfg_ptr;
      uint8_t len;
      uint8_t version;
      uint8_t checksum;
      uint8_t mp_feat1;
      uint8_t mp_feat2;
      uint8_t mp_feat3;
      uint8_t mp_feat4;
      uint8_t mp_feat5;
    } __packed;


    struct mp_table_entry_cpu {
      uint8_t type;
      uint8_t lapic_id;
      uint8_t lapic_version;
      uint8_t enabled : 1;
      uint8_t is_bsp : 1;
      uint8_t unused : 6;
      uint32_t sig;
      uint32_t feat_flags;
      uint32_t rsvd0;
      uint32_t rsvd1;
    } __packed;

    struct mp_table_entry_bus {
      uint8_t type;
      uint8_t bus_id;
      char bus_type_string[6];
    } __packed;

    struct mp_table_entry_ioapic {
      uint8_t type;
      uint8_t id;
      uint8_t version;
      uint8_t enabled : 1;
      uint8_t unused : 7;
      uint32_t addr;
    } __packed;

    struct mp_table_entry_lint {
      uint8_t type;
      uint8_t int_type;
      uint16_t int_flags;
      uint8_t src_bus_id;
      uint8_t src_bus_irq;
      uint8_t dst_lapic_id;
      uint8_t dst_lapic_lintin;
    } __packed;

    struct mp_table_entry_ioint {
      uint8_t type;
      uint8_t int_type;
      uint16_t int_flags;
      uint8_t src_bus_id;
      uint8_t src_bus_irq;
      uint8_t dst_ioapic_id;
      uint8_t dst_ioapic_intin;
    } __packed;


    struct mp_table_entry {
      union {
        struct mp_table_entry_cpu cpu;
        struct mp_table_entry_ioapic ioapic;
      } __packed;
    } __packed;

    struct mp_table {
      uint32_t sig;
      uint16_t len;  /* base table length */
      uint8_t rev;   /* spec revision */
      uint8_t cksum; /* checksum */
      char oem_id[8];
      char prod_id[12];
      uint32_t oem_table_ptr;
      uint16_t oem_table_size;
      uint16_t entry_cnt;
      uint32_t lapic_addr;
      uint16_t ext_table_len;
      uint8_t ext_table_checksum;
      uint8_t rsvd;

      struct mp_table_entry entries[0];

    } __packed;
  }  // namespace mp



  // every CPU has one of these. Stored globally and accessed by the
  // smp::get_state function
  struct cpu_state {
    int index;
    smp::mp::mp_table_entry_cpu *entry;
  };

  // return the current CPU's state.
  cpu_state &get_state(void);
  // and the state of another CPU
  cpu_state &get_state(int apicid);

  // initialize SMP, but don't IPI to the other CPUs yet.
  bool init(void);


  void lapic_init(void);
  void lapic_write(int ind, int value);
  unsigned lapic_read(int ind);
  void lapic_eoi(void);

  void init_cores(void);

  int cpunum(void);

  void ioapicenable(int irq, int cpu);


};  // namespace smp
