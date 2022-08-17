#pragma once

#include <ck/map.h>
#include <ck/string.h>
#include <types.h>
#include <ck/vec.h>



namespace dtb {


  struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
  };


  struct reg {
    off_t address;
    size_t length;
  };

#define DTB_MAX_COMPATIBLE 8

  struct node {
    // name@address
    char name[32];
    off_t address;
    struct dtb::node *parent;


    int ncompat;
    char *compatible[DTB_MAX_COMPATIBLE];
    // the local copy of the compatible buffer.
    char compat[128];
    bool is_device;

    short address_cells;
    short size_cells;
    short irq;

    off_t fdt_offset;

    dtb::reg reg;

    /* The list of children */
    struct dtb::node *children; /* A pointer to the start of a sibling list */
    struct dtb::node *sibling;  /* A pointer to the next sibling of the parent's children */
    inline int get_addr_cells(void) {
      if (address_cells != -1) return address_cells;
      if (parent != NULL) return parent->get_addr_cells();
      return 0;
    }

    inline int get_size_cells(void) {
      if (size_cells != -1) return size_cells;
      if (parent != NULL) return parent->get_size_cells();
      return 0;
    }
  };



  /* Return the number of devices nodes found */
  int parse(dtb::fdt_header *hdr);
  /* Walk the devices with a callback. Continue if the callback returns true */
  void walk_devices(bool (*callback)(dtb::node *), bool all_devices = false);


  // promote the boot representation to the hw::Device representation
  void promote();

};  // namespace dtb
