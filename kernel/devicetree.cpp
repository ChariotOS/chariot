#include <devicetree.h>
#include <vec.h>



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


static struct {
  const char *name;
  int type;
} fdt_types[] = {
    {"compatible", FDT_T_STRING}, {"model", FDT_T_STRING},       {"phandle", FDT_T_INT},
    {"status", FDT_T_STRING},     {"#address-cells", FDT_T_INT}, {"#size-cells", FDT_T_INT},
    {"reg", FDT_T_INT},           {"virtual-reg", FDT_T_INT},    {"ranges", FDT_T_EMPTY},
    {"dma-ranges", FDT_T_EMPTY}, /* TODO: <prop-encoded-array> */
    {"name", FDT_T_STRING},       {"device_type", FDT_T_STRING}, {0, 0},
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
        for (int i = 0; i < len; i++) sp++;
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

        for (int i = 0; i < round_up(len, 4) / 4; i++) sp++;

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
  for (int i = 0; i < depth; i++) printk("    ");
  printk("%s {\n", name.get());

  for (auto &propkv : props) {
    for (int i = 0; i < depth + 1; i++) printk("    ");

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


  for (auto *c : children) c->dump(depth + 1);
  for (int i = 0; i < depth; i++) printk("    ");
  printk("};\n\n");
}
