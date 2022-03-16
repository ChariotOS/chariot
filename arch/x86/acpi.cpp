#include "acpi/acpi.h"

#include <multiboot2.h>
#include <printk.h>
#include <ck/string.h>
#include <util.h>
#include <ck/vec.h>
#include <errno.h>
#include <x86/smp.h>

#define ACPI_LOG(...) PFXLOG(RED "ACPI", __VA_ARGS__)

static ck::vec<acpi_table_header *> acpi_tables;

struct srat_table {
  char signature[4];    // Contains "SRAT"
  uint32_t length;      // Length of entire SRAT including entries
  uint8_t rev;          // 3
  uint8_t checksum;     // Entire table must sum to zero
  uint8_t OEMID[6];     // What do you think it is?
  uint64_t OEMTableID;  // For the SRAT it's the manufacturer model ID
  uint32_t OEMRev;      // OEM revision for OEM Table ID
  uint32_t creatorID;   // Vendor ID of the utility used to create the table
  uint32_t creatorRev;  // Blah blah

  uint8_t reserved[12];
} __attribute__((packed));

struct srat_subtable {
  uint8_t type;
  uint8_t length;
  // ...
};

struct srat_proc_lapic_struct {
  uint8_t type;       // 0x0 for this type of structure
  uint8_t length;     // 16
  uint8_t lo_DM;      // Bits [0:7] of the proximity domain
  uint8_t APIC_ID;    // Processor's APIC ID
  uint32_t flags;     // Haha the most useless thing ever
  uint8_t SAPIC_EID;  // The processor's local SAPIC EID. Don't even bother.
  uint8_t hi_DM[3];   // Bits [8:31] of the proximity domain
  uint32_t _CDM;      // The clock domain which the processor belongs to (more jargon)
} __attribute__((packed));

struct srat_mem_struct {
  uint8_t type;          // 0x1 for this type of structure
  uint8_t length;        // 40
  uint32_t domain;       // The domain to which this memory region belongs to
  uint8_t reserved1[2];  // Reserved
  uint32_t lo_base;      // Low 32 bits of the base address of the memory range
  uint32_t hi_base;      // High 32 bits of the base address of the memory range
  uint32_t lo_length;    // Low 32 bits of the length of the range
  uint32_t hi_length;    // High 32 bits of the length
  uint8_t reserved2[4];  // Reserved
  uint32_t flags;        // Flags
  uint8_t reserved3[8];  // Reserved
} __attribute__((packed));



template <typename Fn>
int acpi_table_parse_entries(struct acpi_table_header *table_header, int table_size, int entry_id, Fn handler) {
  ck::string id = ck::string(table_header->signature, 4);
  struct acpi_subtable_header *entry;
  unsigned int count = 0;
  unsigned long table_end;
  acpi_size tbl_size;
  int max_entries = 255;

  table_end = (unsigned long)table_header + table_header->length;

  /* Parse all entries looking for a match. */

  entry = (struct acpi_subtable_header *)((unsigned long)table_header + table_size);

  while (((unsigned long)entry) + sizeof(struct acpi_subtable_header) < table_end) {
    if (entry->type == entry_id && (!max_entries || count++ < max_entries))
      if (handler(entry, table_end)) {
        return -EINVAL;
      }

    entry = (struct acpi_subtable_header *)((unsigned long)entry + entry->length);
  }
  if (max_entries && count > max_entries) {
    ACPI_LOG(
        "[%4.4s:0x%02x] ignored %i entries of "
        "%i found\n",
        id.get(), entry_id, count - max_entries, count);
  }
  return count;
}

bool acpi::init(uint64_t mbd) {
  struct acpi_table_rsdp *rsdp = NULL;
  if (rsdp == NULL) {
    auto *oacpi = mb2::find<struct multiboot_tag_old_acpi>(mbd, MULTIBOOT_TAG_TYPE_ACPI_OLD);
    if (oacpi != NULL) {
      rsdp = (acpi_table_rsdp *)oacpi->rsdp;
    }
  }

  if (rsdp == NULL) {
    auto *nacpi = mb2::find<struct multiboot_tag_new_acpi>(mbd, MULTIBOOT_TAG_TYPE_ACPI_NEW);
    if (nacpi != NULL) {
      rsdp = (acpi_table_rsdp *)nacpi->rsdp;
    }
  }

  if (rsdp != NULL) {
    if (rsdp->revision > 1) {
      auto *xsdt = (struct acpi_table_xsdt *)p2v((uint64_t)rsdp->xsdt_physical_address);
      int count = (xsdt->header.length - sizeof(xsdt->header)) / 8;

      for (int i = 0; i < count; i++) {
        auto *hdr = (struct acpi_table_header *)p2v((uint64_t)xsdt->table_offset_entry[i]);
        acpi_tables.push(hdr);
      }

    } else {
      auto *rsdt = (struct acpi_table_rsdt *)p2v((uint64_t)rsdp->rsdt_physical_address);
      int count = (rsdt->header.length - sizeof(rsdt->header)) / 4;

      for (int i = 0; i < count; i++) {
        auto *hdr = (struct acpi_table_header *)p2v((uint64_t)rsdt->table_offset_entry[i]);
        acpi_tables.push(hdr);
      }
    }
  }

  {
    int i = 0;
    for (auto *tbl : acpi_tables) {
      ck::string sig = ck::string(tbl->signature, ACPI_NAME_SIZE);
      ck::string oem = ck::string(tbl->oem_id, ACPI_OEM_ID_SIZE);
      // ACPI_LOG("[ACPI] table[%d]: sig: '%s', oem: '%s'\n", i++, sig.get(), oem.get());

      // MADT Structure
      if (sig == "APIC") {
        auto *p = (struct acpi_table_madt *)tbl;
        ACPI_LOG("Parsing MADT...\n");
        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_LOCAL_APIC, [](struct acpi_subtable_header *hdr, const long end) {
          struct acpi_madt_local_apic *p = (struct acpi_madt_local_apic *)hdr;
          smp::add_cpu(p->id);
          ACPI_LOG("found cpu #%d,%d\n", p->id, p->processor_id);
          return 0;
        });



        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_LOCAL_X2APIC, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found local x2apic\n");
          return 0;
        });


        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_IO_APIC, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found io apic\n");
          return 0;
        });


        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_INTERRUPT_SOURCE, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found interrupt source\n");
          return 0;
        });

        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_INTERRUPT_OVERRIDE, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found interrupt source\n");
          return 0;
        });


        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_NMI_SOURCE, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found an nmi source\n");
          return 0;
        });


        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_LOCAL_APIC_NMI, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found a local apic NMI\n");
          return 0;
        });

        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found a local apic override\n");
          return 0;
        });

        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_IO_SAPIC, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found an IO SAPIC\n");
          return 0;
        });

        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_LOCAL_SAPIC, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found a Local SAPIC\n");
          return 0;
        });

        acpi_table_parse_entries(tbl, sizeof(*p), ACPI_MADT_TYPE_LOCAL_X2APIC_NMI, [](struct acpi_subtable_header *hdr, const long end) {
          ACPI_LOG("found a local x2apic NMI\n");
          return 0;
        });
      }


      // FADT Structure... for some reason it has the signature of FACP...
      if (sig == "FACP") {
        auto *p = (struct acpi_table_fadt *)tbl;

        ACPI_LOG("Parsing FADT...\n");
        ACPI_LOG("system interrupt model: %x\n", p->model);
        ACPI_LOG("boot_flags: %x\n", p->boot_flags);


        int legacy = p->boot_flags & ACPI_FADT_LEGACY_DEVICES;
        int i8042 = p->boot_flags & ACPI_FADT_8042;
        int novga = p->boot_flags & ACPI_FADT_NO_VGA;
        int nomsi = p->boot_flags & ACPI_FADT_NO_MSI;

        ACPI_LOG("flags are: legacy=%d i8042=%d novga=%d nomsi=%d\n", legacy, i8042, novga, nomsi);
      }

      if (sig == "HPET") {
        auto *hpet_tbl = (struct acpi_table_hpet *)tbl;

        ACPI_LOG("[ACPI:HPET] Initializing HPET Device\n", (void *)hpet_tbl);
        ACPI_LOG("[ACPI:HPET]\tID: 0x%x\n", hpet_tbl->id);
        ACPI_LOG("[ACPI:HPET]\tBase Address: %p\n", (void *)hpet_tbl->address.address);
        ACPI_LOG("[ACPI:HPET]\t\tAccess Width: %u\n", hpet_tbl->address.access_width);
        ACPI_LOG("[ACPI:HPET]\t\tBit Width:    %u\n", hpet_tbl->address.bit_width);
        ACPI_LOG("[ACPI:HPET]\t\tBit Offset:   %u\n", hpet_tbl->address.bit_offset);
        ACPI_LOG("[ACPI:HPET]\t\tAspace ID:  0x%x\n", hpet_tbl->address.space_id);
        ACPI_LOG("[ACPI:HPET]\tSeq Num: 0x%x\n", hpet_tbl->sequence);
        ACPI_LOG("[ACPI:HPET]\tMinimum Tick Rate %u\n", hpet_tbl->minimum_tick);
        ACPI_LOG("[ACPI:HPET]\tFlags: 0x%x\n", hpet_tbl->flags);
      }


      if (sig == "SRAT") {
        char *end = (char *)tbl + tbl->length;

        struct srat_table *srat = (struct srat_table *)tbl;

        int i = 0;
        for (char *s = (char *)(srat + 1); s < end;) {
          auto *stbl = (struct srat_subtable *)s;

          s += stbl->length;
          switch (stbl->type) {
            case ACPI_SRAT_TYPE_CPU_AFFINITY: {
              auto *p = (struct acpi_srat_cpu_affinity *)stbl;
              uint32_t proximity_domain = p->proximity_domain_lo;

              if (srat->rev >= 2) {
                proximity_domain |= p->proximity_domain_hi[0] << 8;
                proximity_domain |= p->proximity_domain_hi[1] << 16;
                proximity_domain |= p->proximity_domain_hi[2] << 24;
              }
              ACPI_LOG("[ACPI:SRAT] Processor (id[0x%02x] eid[0x%02x]) in proximity domain %d %s\n", p->apic_id, p->local_sapic_eid,
                  proximity_domain, p->flags & ACPI_SRAT_CPU_ENABLED ? "enabled" : "disabled");
              break;
            }

            case ACPI_SRAT_TYPE_MEMORY_AFFINITY: {
              auto *p = (struct acpi_srat_mem_affinity *)stbl;
              ACPI_LOG(
                  "[ACPI:SRAT] Memory (0x%llx length 0x%llx type 0x%x) in proximity domain %d "
                  "%s%s\n",
                  p->base_address, p->length, p->memory_type, p->proximity_domain,
                  p->flags & ACPI_SRAT_MEM_ENABLED ? "enabled" : "disabled",
                  p->flags & ACPI_SRAT_MEM_HOT_PLUGGABLE ? " hot-pluggable" : "");
              break;
            }


            case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY: {
              struct acpi_srat_x2apic_cpu_affinity *p = (struct acpi_srat_x2apic_cpu_affinity *)stbl;
              ACPI_LOG("[ACPI:SRAT] Processor (x2apicid[0x%08x]) in proximity domain %d %s\n", p->apic_id, p->proximity_domain,
                  (p->flags & ACPI_SRAT_CPU_ENABLED) ? "enabled" : "disabled");
              break;
            }
          }
        }
      }
    }
  }

  return true;
}
