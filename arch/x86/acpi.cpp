#include "acpi/acpi.h"
#include <util.h>
#include <multiboot2.h>
#include <printk.h>
#include <vec.h>
#include <string.h>


static vec<acpi_table_header*> acpi_tables;

bool acpi::init(uint64_t mbd) {

	struct acpi_table_rsdp *rsdp = NULL;
	if (rsdp == NULL) {
		auto *oacpi = mb2::find<struct multiboot_tag_old_acpi>(mbd, MULTIBOOT_TAG_TYPE_ACPI_OLD);
		if (oacpi != NULL) {
			rsdp = (acpi_table_rsdp*)oacpi->rsdp;
		}
	}

	if (rsdp == NULL) {
		auto *nacpi = mb2::find<struct multiboot_tag_new_acpi>(mbd, MULTIBOOT_TAG_TYPE_ACPI_NEW);
		if (nacpi != NULL) {
			rsdp = (acpi_table_rsdp*)nacpi->rsdp;
		}
	}

	if (rsdp != NULL) {
		if (rsdp->revision > 1) {
			auto *xsdt = (struct acpi_table_xsdt*)p2v((uint64_t)rsdp->xsdt_physical_address);
			hexdump(xsdt, xsdt->header.length, true);
			int count = (xsdt->header.length - sizeof(xsdt->header)) / 8;

			for (int i = 0; i < count; i++) {
				auto *hdr = (struct acpi_table_header*)p2v((uint64_t)xsdt->table_offset_entry[i]);
				acpi_tables.push(hdr);
			}

		} else {
			auto *rsdt = (struct acpi_table_rsdt*)p2v((uint64_t)rsdp->rsdt_physical_address);
			hexdump(rsdt, rsdt->header.length, true);
			int count = (rsdt->header.length - sizeof(rsdt->header)) / 4;

			for (int i = 0; i < count; i++) {
				auto *hdr = (struct acpi_table_header*)p2v((uint64_t)rsdt->table_offset_entry[i]);
				acpi_tables.push(hdr);
			}
		}
		// hexdump(p2v((uint64_t)rsdp->rsdt_physical_address), rsdp->length, true);
	}

	int i = 0;
	for (auto *tbl : acpi_tables) {
		string sig = string(tbl->signature, ACPI_NAME_SIZE);
		string oem = string(tbl->oem_id, ACPI_OEM_ID_SIZE);
		debug("[ACPI] table[%d]: sig: '%s', oem: '%s'\n", i++, sig.get(), oem.get());
	}

  return true;
}

