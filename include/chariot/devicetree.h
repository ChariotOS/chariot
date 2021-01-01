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


  struct device_tree {
    struct node {
      string name;
      struct node *parent = NULL;

      /*
       * Some of the standard props
       * source: https://devicetree-specification.readthedocs.io/en/v0.2/devicetree-basics.html#sect-property-values
       */
      map<string, vec<uint8_t>> props;

      vec<struct node *> children;

      inline void set_prop(string name, int vlen, void *value) {
        vec<uint8_t> v;
        for (int i = 0; i < vlen; i++) v.push(((uint8_t *)value)[i]);
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

