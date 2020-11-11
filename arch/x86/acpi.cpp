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
    }
  }

  for (auto *tbl : acpi_tables) {
    if (!memcmp(tbl->signature, "SRAT", 4)) {
      char *end = (char *)tbl + tbl->length;

      struct srat_table *srat = (struct srat_table *)tbl;

      int i = 0;
      for (char *s = (char *)(srat + 1); s < end;) {
        auto *stbl = (struct srat_subtable *)s;

        s += stbl->length;
        switch (stbl->type) {
          case 0: {
            auto *t = (struct srat_proc_lapic_struct *)stbl;
            uint32_t prox_domain = 0;
            prox_domain |= t->lo_DM;
            prox_domain |= t->hi_DM[0] << 8;
            prox_domain |= t->hi_DM[1] << 16;
            prox_domain |= t->hi_DM[2] << 24;
            debug("[SRAT] lapic - apic id: %d, prox domain: %08x\n", t->APIC_ID, prox_domain);
            break;
          }

          case 1: {
            auto *t = (struct srat_mem_struct *)stbl;
						off_t start = t->lo_base | (off_t)t->hi_base << 32;
						off_t length = t->lo_length | (off_t)t->hi_length << 32;
            debug("[SRAT] mem - domain: %d 0x%llx - 0x%llx\n", t->domain, start, start + length);
            break;
          }
        }
      }
    }
  }

  return true;
}

