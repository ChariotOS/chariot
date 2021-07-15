#include <devicetree.h>
#include <util.h>
#include <ck/vec.h>

//! Byte swap int
static int32_t b2l(int32_t val) {
  val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
  return (val << 16) | ((val >> 16) & 0xFFFF);
}

/* Bigendian uint32_t pointer. Used for parsing the structure block nicely */
struct be32p_t {
 private:
  uint32_t *p;

 public:
  be32p_t(uint32_t *p) : p(p) {}

  uint32_t operator*(void) { return b2l(*p); }

  uint32_t *get(void) { return p; };

  auto &operator++(int) {
    p++;
    return *this;
  }
};


#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

#define FDT_T_STRING 0
#define FDT_T_INTARR 1
#define FDT_T_INT 2
#define FDT_T_EMPTY 4

#define FDT_NUM_NODES 64

static dtb::fdt_header *global_fdt_header = NULL;
static dtb::node devices[FDT_NUM_NODES];
static int next_device = 0;

static dtb::node *alloc_device(const char *name) {
  auto *dev = &devices[next_device++];
  memset(dev, 0, sizeof(*dev));
  strncpy(dev->name, name, 32);
  dev->is_device = false;
  dev->address_cells = dev->size_cells = -1;

  int at_off = -1;
  for (int i = 0; true; i++) {
    if (name[i] == 0) {
      /* reached the end, didn't find an @ */
      break;
    }

    if (name[i] == '@') {
      at_off = i;
      break;
    }
  }

  if (at_off != -1) {
    if (sscanf(name + at_off + 1, "%x", &dev->address) != 1) {
      // printk("INVALID NODE NAME: '%s'\n", name);
    }
    dev->name[at_off] = 0;
  }
  return dev;
}

#define STREQ(s1, s2) (!strcmp((s1), (s2)))

static void node_set_prop(dtb::node *node, const char *name, int len, uint8_t *val) {
  // printk("%s.%s\n", node->name, name);
  if (STREQ(name, "#address-cells")) {
    node->address_cells = *be32p_t((uint32_t *)val);
    return;
  }

  if (STREQ(name, "#size-cells")) {
    node->size_cells = *be32p_t((uint32_t *)val);
    return;
  }
  if (STREQ(name, "interrupts")) {
    int size_cells = node->get_size_cells();
    node->irq = __builtin_bswap32(*(uint32_t *)val);
    return;
  }

  if (STREQ(name, "reg")) {
    node->is_device = true;
    /* TODO: It's unsafe to assume 64 bit here... But since we are 64bit only... (for now) */
    auto *cells = (unsigned long *)val;

    int addr_cells = node->get_addr_cells();
    int size_cells = node->get_size_cells();

    // printk("%s: addr %d, size %d\n", node->name, addr_cells, size_cells);
    if (addr_cells > 0) {
      if (addr_cells == 1) node->reg.address = __builtin_bswap32(*(uint32_t *)val);
      if (addr_cells == 2) node->reg.address = __builtin_bswap64(*(uint64_t *)val);
    }
    val += addr_cells * 4;

    if (size_cells > 0) {
      if (size_cells == 1) node->reg.length = __builtin_bswap32(*(uint32_t *)val);
      if (size_cells == 2) node->reg.length = __builtin_bswap64(*(uint64_t *)val);
    }
    val += size_cells * 4;

    return;
  }

  if (STREQ(name, "compatible")) {
    node->is_device = true;
    strncpy(node->compatible, (const char *)val, sizeof(node->compatible));
    // printk("compatible: %s\n", val);

    return;
  }
  // printk("   %s@%llx\t%s = %p\n", node->name, node->address, name, val);
}


void dtb::walk_devices(bool (*callback)(dtb::node *)) {
  for (int i = 0; i < next_device; i++) {
    if (devices[i].is_device) {
      if (!callback(&devices[i])) break;
    }
  }
}

static void spaces(int n) {
  for (int i = 0; i < n; i++)
    printk("  ");
}

void dump_dtb(dtb::node *node, int depth = 0) {
  spaces(depth);
  printk("%s@%llx\n", node->name, node->address);
  if (node->address_cells != -1) {
    spaces(depth);
    printk("- #address-cells: %d\n", node->address_cells);
  }
  if (node->size_cells != -1) {
    spaces(depth);
    printk("- #size-cells: %d\n", node->size_cells);
  }

  spaces(depth);
  printk("- compatible: %s\n", node->compatible);


  if (node->is_device) {
    spaces(depth);
    printk("- reg: %p %zu\n", node->reg.address, node->reg.length);
  }
  for (auto *cur = node->children; cur != NULL; cur = cur->sibling) {
    dump_dtb(cur, depth + 1);
  }
}

int dtb::parse(dtb::fdt_header *fdt) {
  printk(KERN_INFO "fdt at %p\n", fdt);
  global_fdt_header = fdt;

  assert(next_device == 0);
  auto *root = alloc_device("");

  auto *node = root;
  dtb::node *new_node = NULL;


  be32p_t sp = (uint32_t *)((off_t)fdt + b2l(fdt->off_dt_struct));
  const char *strings = (const char *)((off_t)fdt + b2l(fdt->off_dt_strings));
  int depth = 0;

  while (*sp != FDT_END) {
    uint32_t op = *sp;
    /* sp points to the next word */
    sp++;

    uint32_t len;
    uint32_t nameoff;
    const char *name;
    const char *valptr = NULL;
    char value[256];

    switch (op) {
      case FDT_BEGIN_NODE:
        name = (const char *)sp.get();

        len = round_up(strlen(name) + 1, 4) / 4;
        for (int i = 0; i < len; i++)
          sp++;
        new_node = alloc_device(name);

        new_node->parent = node;

        new_node->sibling = node->children;
        node->children = new_node;

        node = new_node;
        new_node = NULL;

        depth++;

        break;
      case FDT_END_NODE:
        depth--;
        node = node->parent;
        break;

      case FDT_PROP:
        len = *sp;
        sp++;
        nameoff = *sp;
        sp++;
        valptr = (const char *)sp.get();
        node_set_prop(node, strings + nameoff, len, (uint8_t *)valptr);
        for (int i = 0; i < round_up(len, 4) / 4; i++)
          sp++;

        break;

      case FDT_NOP:
        // printk("nop\n");
        break;

      case FDT_END:
        // printk("end\n");
        break;
    }
  }

  // dump_dtb(node, 0);
  return next_device;
}




static struct {
  const char *name;
  int type;
} fdt_types[] = {
    {"compatible", FDT_T_STRING},
    {"model", FDT_T_STRING},
    {"phandle", FDT_T_INT},
    {"status", FDT_T_STRING},
    {"#address-cells", FDT_T_INT},
    {"#size-cells", FDT_T_INT},
    {"reg", FDT_T_INT},
    {"virtual-reg", FDT_T_INT},
    {"ranges", FDT_T_EMPTY},
    {"dma-ranges", FDT_T_EMPTY}, /* TODO: <prop-encoded-array> */
    {"name", FDT_T_STRING},
    {"device_type", FDT_T_STRING},
    {0, 0},
};

static int get_fdt_prop_type(const char *c) {
  auto *m = &fdt_types[0];

  while (m->name != NULL) {
    if (!strcmp(c, m->name)) return m->type;
    m++;
  }
  return -1;
}

dtb::device_tree::device_tree(struct dtb::fdt_header *hdr) {
  fdt = (struct fdt_header *)malloc(b2l(hdr->totalsize));
  memcpy(fdt, hdr, b2l(hdr->totalsize));

  be32p_t sp = (uint32_t *)((off_t)fdt + b2l(fdt->off_dt_struct));
  const char *strings = (const char *)((off_t)fdt + b2l(fdt->off_dt_strings));
  int depth = 0;

  auto *node = &root;

  while (*sp != FDT_END) {
    uint32_t op = *sp;
    /* sp points to the next word */
    sp++;

    uint32_t len;
    uint32_t nameoff;
    const char *name;
    const char *valptr = NULL;
    char value[256];

    switch (op) {
      case FDT_BEGIN_NODE:
        name = (const char *)sp.get();

        len = round_up(strlen(name) + 1, 4) / 4;
        for (int i = 0; i < len; i++)
          sp++;
        node = node->spawn(name);
        depth++;

        break;
      case FDT_END_NODE:
        depth--;
        node = node->parent;
        break;

      case FDT_PROP:
        len = *sp;
        sp++;
        nameoff = *sp;
        sp++;
        valptr = (const char *)sp.get();
        node->set_prop(strings + nameoff, len, (uint8_t *)valptr);

        for (int i = 0; i < round_up(len, 4) / 4; i++)
          sp++;

        break;

      case FDT_NOP:
        // printk("nop\n");
        break;

      case FDT_END:
        // printk("end\n");
        break;
    }
  }
}


dtb::device_tree::~device_tree(void) { free((void *)fdt); }

void dtb::device_tree::node::dump(int depth) {
  for (int i = 0; i < depth; i++)
    printk("    ");
  printk("%s {\n", name.get());

  for (auto &propkv : props) {
    for (int i = 0; i < depth + 1; i++)
      printk("    ");

    void *raw = (void *)propkv.value.data();
    auto len = propkv.value.size();

    printk("%s", propkv.key.get());
    switch (get_fdt_prop_type(propkv.key.get())) {
      case FDT_T_STRING:
        printk(" = '%s'", (const char *)raw);
        break;

      case FDT_T_INTARR:
        for (int i = 0; i < len / 4; i++) {
          printk(" = <0x%x>", b2l(*((uint32_t *)raw) + i));
        }
        break;
      case FDT_T_INT:
        printk(" = <0x%x>", b2l(*(uint32_t *)raw));
        break;

      case FDT_T_EMPTY:
        break;
      default:
        printk(" = ?");
        break;
    }

    printk("\n");
  }


  for (auto *c : children)
    c->dump(depth + 1);
  for (int i = 0; i < depth; i++)
    printk("    ");
  printk("};\n\n");
}
