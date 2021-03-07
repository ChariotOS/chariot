#include "acpi/acpi.h"
#include <multiboot2.h>
#include <printk.h>
#include <string.h>
#include <util.h>
#include <vec.h>


static vec<acpi_table_header *> acpi_tables;

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
      string sig = string(tbl->signature, ACPI_NAME_SIZE);
      string oem = string(tbl->oem_id, ACPI_OEM_ID_SIZE);
      debug("[ACPI] table[%d]: sig: '%s', oem: '%s'\n", i++, sig.get(), oem.get());


      if (!memcmp(tbl->signature, "HPET", 4)) {
        auto *hpet_tbl = (struct acpi_table_hpet *)tbl;

        debug("[ACPI:HPET] Initializing HPET Device\n", (void *)hpet_tbl);
        debug("[ACPI:HPET]\tID: 0x%x\n", hpet_tbl->id);
        debug("[ACPI:HPET]\tBase Address: %p\n", (void *)hpet_tbl->address.address);
        debug("[ACPI:HPET]\t\tAccess Width: %u\n", hpet_tbl->address.access_width);
        debug("[ACPI:HPET]\t\tBit Width:    %u\n", hpet_tbl->address.bit_width);
        debug("[ACPI:HPET]\t\tBit Offset:   %u\n", hpet_tbl->address.bit_offset);
        debug("[ACPI:HPET]\t\tAspace ID:  0x%x\n", hpet_tbl->address.space_id);
        debug("[ACPI:HPET]\tSeq Num: 0x%x\n", hpet_tbl->sequence);
        debug("[ACPI:HPET]\tMinimum Tick Rate %u\n", hpet_tbl->minimum_tick);
        debug("[ACPI:HPET]\tFlags: 0x%x\n", hpet_tbl->flags);
      }


      if (!memcmp(tbl->signature, "SRAT", 4)) {
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
              debug("[ACPI:SRAT] Processor (id[0x%02x] eid[0x%02x]) in proximity domain %d %s\n",
                    p->apic_id, p->local_sapic_eid, proximity_domain,
                    p->flags & ACPI_SRAT_CPU_ENABLED ? "enabled" : "disabled");
              break;
            }

            case ACPI_SRAT_TYPE_MEMORY_AFFINITY: {
              auto *p = (struct acpi_srat_mem_affinity *)stbl;
              debug(
                  "[ACPI:SRAT] Memory (0x%llx length 0x%llx type 0x%x) in proximity domain %d "
                  "%s%s\n",
                  p->base_address, p->length, p->memory_type, p->proximity_domain,
                  p->flags & ACPI_SRAT_MEM_ENABLED ? "enabled" : "disabled",
                  p->flags & ACPI_SRAT_MEM_HOT_PLUGGABLE ? " hot-pluggable" : "");
              break;
            }


            case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY: {
              struct acpi_srat_x2apic_cpu_affinity *p =
                  (struct acpi_srat_x2apic_cpu_affinity *)stbl;
              debug("[ACPI:SRAT] Processor (x2apicid[0x%08x]) in proximity domain %d %s\n",
                    p->apic_id, p->proximity_domain,
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
