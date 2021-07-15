#pragma once

#include <map.h>
#include <string.h>
#include <types.h>
#include <vec.h>

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

  struct node {
    // name@address
    char name[32];
    off_t address;
    struct dtb::node *parent;
    char compatible[32];
    bool is_device;

    short address_cells;
    short size_cells;
    short irq;

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
  void walk_devices(bool (*callback)(dtb::node *));

  struct device_tree {
    struct node {
      string name;
      struct node *parent = NULL;

      /*
       * Some of the standard props
       * source:
       * https://devicetree-specification.readthedocs.io/en/v0.2/devicetree-basics.html#sect-property-values
       */
      ck::map<string, ck::vec<uint8_t>> props;

      ck::vec<struct node *> children;

      inline void set_prop(string name, int vlen, void *value) {
        ck::vec<uint8_t> v;
        for (int i = 0; i < vlen; i++)
          v.push(((uint8_t *)value)[i]);
        props[name] = move(v);
      }

      inline struct node *spawn(const char *name) {
        auto n = new node;
        n->name = name;
        n->parent = this;
        children.push(n);
        return n;
      }

      void dump(int depth = 0);
    };

    device_tree(struct fdt_header *fdt);
    ~device_tree(void);

    inline void dump(void) { root.dump(); }

    struct fdt_header *fdt = NULL;
    struct node root;
  };

};  // namespace dtb
