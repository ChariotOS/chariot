#include <devicetree.h>
#include <util.h>
#include <syscall.h>
#include <dev/hardware.h>
#include <ck/vec.h>


#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

#define FDT_T_STRING 0
#define FDT_T_INTARR 1
#define FDT_T_INT 2
#define FDT_T_EMPTY 4

#define FDT_NUM_NODES 75

#define LOG(...) PFXLOG(YEL "FDT", __VA_ARGS__)

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


void dtb::walk_devices(bool (*callback)(dtb::node *)) {
  for (int i = 0; i < next_device; i++) {
    if (devices[i].is_device) {
      if (!callback(&devices[i])) break;
    }
  }
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

  if (STREQ(name, "compatible")) {
    node->is_device = true;
    memcpy(node->compat, (const char *)val, len);


    node->ncompat = 0;
    char *cur = node->compat;
    off_t off = 0;
    while (off < len) {
      if (cur[0] == '\0') break;
      // grab a strlen
      size_t current_len = strlen(cur);
      node->compatible[node->ncompat] = cur;
      node->ncompat += 1;

      off += current_len + 1;
      cur = node->compat + off;
    }


    return;
  }
}




int dtb::parse(dtb::fdt_header *fdt) {
  LOG("Parsing device tree at %p, size: %zu\n", fdt, b2l(fdt->totalsize));
  global_fdt_header = fdt;

  assert(next_device == 0);
  auto *root = alloc_device("");

  auto *node = root;
  dtb::node *new_node = NULL;

  be32p_t sp = (uint32_t *)((off_t)fdt + b2l(fdt->off_dt_struct));
  const char *strings = (const char *)((off_t)fdt + b2l(fdt->off_dt_strings));

  int depth = 0;

  while (*sp != FDT_END) {
    off_t off = ((off_t)sp.get() - (off_t)fdt);
    uint32_t op = *sp;
    /* sp points to the next word */
    sp++;

    uint32_t len;
    uint32_t nameoff;
    const char *name;
    const char *valptr = NULL;

    switch (op) {
      case FDT_BEGIN_NODE: {
        name = (const char *)sp.get();

        len = round_up(strlen(name) + 1, 4) / 4;
        for (int i = 0; i < len; i++)
          sp++;
        new_node = alloc_device(name);
        new_node->parent = node;
        new_node->fdt_offset = off;
        new_node->sibling = node->children;
        node->children = new_node;
        node = new_node;
        new_node = NULL;
        depth++;
        break;
      }
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
        break;
      case FDT_END:
        break;
    }
  }
  return next_device;
}




static struct {
  const char *name;
  hw::Prop::Type type;
} fdt_types[] = {
    {"compatible", hw::Prop::Type::String},
    {"model", hw::Prop::Type::String},
    {"stdout-path", hw::Prop::Type::String},
    {"bootargs", hw::Prop::Type::String},
    {"riscv,isa", hw::Prop::Type::String},
    {"phandle", hw::Prop::Type::Integer},
    {"status", hw::Prop::Type::String},
    {"#address-cells", hw::Prop::Type::CellSize},
    {"#size-cells", hw::Prop::Type::CellSize},
    {"#interrupt-cells", hw::Prop::Type::CellSize},
    {"reg", hw::Prop::Type::Register},
    {"virtual-reg", hw::Prop::Type::Register},
    {"ranges", hw::Prop::Type::Unknown},
    {"dma-ranges", hw::Prop::Type::Unknown}, /* TODO: <prop-encoded-array> */
    {"name", hw::Prop::Type::String},
    {"device_type", hw::Prop::Type::String},
    {"interrupts", hw::Prop::Type::Interrupts},
    {0, hw::Prop::Unknown},
};

static hw::Prop::Type get_fdt_prop_type(const char *c) {
  auto *m = &fdt_types[0];

  while (m->name != NULL) {
    if (!strcmp(c, m->name)) return m->type;
    m++;
  }
  return hw::Prop::Type::Unknown;
}


class DTBDevice : public hw::MMIODevice {
 public:
  DTBDevice(const char *name, off_t addr) : hw::MMIODevice(addr, 0) { this->set_name(name); }


  static ck::ref<DTBDevice> alloc(const char *name) {
    char buf[64];
    off_t addr = 0;
    strncpy(buf, name, 64);


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
      if (sscanf(name + at_off + 1, "%x", &addr) != 1) {
        // printk("INVALID NODE NAME: '%s'\n", name);
      }
      buf[at_off] = 0;
    }

    auto dev = ck::make_ref<DTBDevice>(buf, addr);
    dev->addr_cells = dev->size_cells = -1;

    return dev;
  }


  virtual ~DTBDevice(void) {}



  void propegate_cell_sizes(void) {
    for (auto &[name, prop] : m_props) {
      prop.size_cells = get_size_cells();
      prop.address_cells = get_addr_cells();
      prop.irq_cells = get_irq_cells();
    }

    for (auto c : children()) {
      ((DTBDevice *)c.get())->propegate_cell_sizes();
    }
  }



  void parse_prop(const char *name, int len, uint8_t *val) {
    auto addr_cells = get_addr_cells();
    auto size_cells = get_size_cells();
    auto irq_cells = get_irq_cells();
    {
      hw::Prop prop;
      prop.type = get_fdt_prop_type(name);
      prop.size_cells = size_cells;
      prop.address_cells = addr_cells;
      prop.irq_cells = irq_cells;

      for (size_t i = 0; i < len; i++) {
        prop.data.push(val[i]);
      }
      m_props.set(name, move(prop));
    }

    if (STREQ(name, "#address-cells")) {
      this->addr_cells = *be32p_t((uint32_t *)val);
      return;
    }
    if (STREQ(name, "#size-cells")) {
      this->size_cells = *be32p_t((uint32_t *)val);
      return;
    }

    if (STREQ(name, "#interrupt-cells")) {
      this->irq_cells = *be32p_t((uint32_t *)val);
      return;
    }


    if (STREQ(name, "interrupts")) {
      // just parse the first one
      if (irq_cells == 1) interrupt = __builtin_bswap32(*(uint32_t *)val);
      if (irq_cells == 2) interrupt = __builtin_bswap64(*(uint64_t *)val);
      return;
    }

    if (STREQ(name, "reg")) {
      /* TODO: It's unsafe to assume 64 bit here... But since we are 64bit only... (for now) */
      auto *cells = (unsigned long *)val;

      // printk("%s: addr %d, size %d\n", node->name, addr_cells, size_cells);
      if (addr_cells > 0) {
        if (addr_cells == 1) reg.address = __builtin_bswap32(*(uint32_t *)val);
        if (addr_cells == 2) reg.address = __builtin_bswap64(*(uint64_t *)val);
      }
      val += addr_cells * 4;

      if (size_cells > 0) {
        if (size_cells == 1) reg.length = __builtin_bswap32(*(uint32_t *)val);
        if (size_cells == 2) reg.length = __builtin_bswap64(*(uint64_t *)val);
      }
      val += size_cells * 4;

      return;
    }

    if (STREQ(name, "compatible")) {
      const char *allcompat = (const char *)val;
      char buf[len];
      memcpy(buf, val, len);
      char *cur = buf;
      off_t off = 0;
      while (off < len) {
        if (cur[0] == '\0') break;
        // grab a strlen
        size_t current_len = strlen(cur);

        compat().push(ck::string(cur, current_len));

        off += current_len + 1;
        cur = buf + off;
      }
      return;
    }
  }


  short get_addr_cells(void) const {
    if (parent()) {
      return ((DTBDevice *)parent().get())->addr_cells;
    }
    return 1;
  }


  short get_size_cells(void) const {
    if (parent()) {
      return ((DTBDevice *)parent().get())->size_cells;
    }
    return 1;
  }


  short get_irq_cells(void) const {
    if (parent()) {
      return ((DTBDevice *)parent().get())->irq_cells;
    }
    return 1;
  }


  int addr_cells = 1;
  int size_cells = 1;
  int irq_cells = 1;
  dtb::reg reg;
};



auto alloc_device_late(const char *name) {}


void dtb::promote(void) {
  // copy the device tree to a place that won't be stomped on by the memory allocator
  auto fdt = (struct fdt_header *)malloc(b2l(global_fdt_header->totalsize));
  memcpy(fdt, global_fdt_header, b2l(global_fdt_header->totalsize));
  global_fdt_header = fdt;

  LOG("Parsing device tree at %p, size: %zu\n", fdt, b2l(fdt->totalsize));
  global_fdt_header = fdt;

  ck::ref<DTBDevice> root = nullptr;
  auto node = root;
  ck::ref<DTBDevice> new_node = nullptr;

  be32p_t sp = (uint32_t *)((off_t)fdt + b2l(fdt->off_dt_struct));
  const char *strings = (const char *)((off_t)fdt + b2l(fdt->off_dt_strings));

  int depth = 0;

  while (*sp != FDT_END) {
    off_t off = ((off_t)sp.get() - (off_t)fdt);
    uint32_t op = *sp;
    /* sp points to the next word */
    sp++;

    uint32_t len;
    uint32_t nameoff;
    const char *name;
    const char *valptr = NULL;

    switch (op) {
      case FDT_BEGIN_NODE: {
        name = (const char *)sp.get();

        len = round_up(strlen(name) + 1, 4) / 4;
        for (int i = 0; i < len; i++)
          sp++;
        new_node = DTBDevice::alloc(name);

        if (node != nullptr) {
          node->adopt(new_node);
        } else {
          root = new_node;
        }
        node = new_node;
        new_node = nullptr;

        depth++;

        break;
      }
      case FDT_END_NODE:
        depth--;
        node = node->parent();
        break;

      case FDT_PROP:
        len = *sp;
        sp++;
        nameoff = *sp;
        sp++;
        valptr = (const char *)sp.get();
        node->parse_prop(strings + nameoff, len, (uint8_t *)valptr);
        for (int i = 0; i < round_up(len, 4) / 4; i++)
          sp++;

        break;

      case FDT_NOP:
      case FDT_END:
        break;
    }
  }


  root->propegate_cell_sizes();
  hw::Device::add("dtb", root);
}

