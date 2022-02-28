#include <dev/hardware.h>
#include <dev/driver.h>
#include <lock.h>
#include <ck/vec.h>
#include <module.h>
#include <util.h>

#define DEVLOG(...) PFXLOG(YEL "DEV", __VA_ARGS__)

static spinlock all_hardware_lock;
static ck::vec<ck::ref<hw::Device>> all_hardware;


hw::Device::Device(DeviceType t) : m_type(t) {}


void hw::Device::attach_to(dev::Device *drv) { this->m_device = drv; }



static void recurse_print(ck::ref<hw::Device> dev, bool props, int depth = 0) {
  auto spaces = [&] {
    if (!props) printk(YEL "DEV" RESET ":");
    for (int i = 0; i < depth; i++)
      printk("  ");
  };

  spaces();
  printk(GRN "%s", dev->name().get());

  if (auto mmio = dev->cast<hw::MMIODevice>()) {
    if (mmio->address() != 0) {
      printk(GRY "@" YEL "%08x", mmio->address());
    }
    for (auto &compat : mmio->compat()) {
      printk(GRY " '%s'", compat.get());
    }

    printk(RESET);
  }

  if (auto device = dev->attached_device()) {
    printk(RED " driven by '%s'", device->driver().name().get());
  }

  if (props) printk(RESET " {");
  printk("\n");

  if (props) {
    for (auto &[name, prop] : dev->props()) {
      spaces();
      auto val = prop.format();
      printk("  " BLU "%s = ", name.get());

      if (val.size() > 0) {
        switch (val[0]) {
          case '<':
            printk(YEL);
            break;
          case '"':
            printk(GRN);
            break;
          default:
            printk(GRY);
            break;
        }
      }
      printk("%s" RESET "\n", val.get());
    }
  }

  for (auto &c : dev->children()) {
    recurse_print(c, props, depth + 1);
  }

  if (props) {
    spaces();

    printk("}\n");
  }
}

static void recurse_probe(ck::ref<hw::Device> dev, int depth = 0) {
  dev->lock();


  for (auto &c : dev->children()) {
    recurse_probe(c, depth + 1);
  }
}




ksh_def("hw", "display all hardware devices and their props") {
  scoped_lock l(all_hardware_lock);
  for (auto &dev : all_hardware) {
    recurse_print(dev, true);
  }
  return 0;
}


// Register a device at the top level
void hw::Device::add(ck::string name, ck::ref<Device> dev) {
  dev->set_name(name);

  recurse_print(dev, false);


  scoped_lock l(all_hardware_lock);
  assert(all_hardware.find(dev).is_end());
  all_hardware.push(dev);

  dev::Driver::probe_all(dev);
}


// Register a device with the global device system
void hw::Device::remove(ck::ref<Device> dev, RemovalReason reason) {
  scoped_lock l(all_hardware_lock);
  // TODO: Notify drivers that the device has been removed, and why
  all_hardware.remove_first_matching([dev](auto other) {
    return other == dev;
  });
}

ck::vec<ck::ref<hw::Device>> hw::Device::all(void) {
  scoped_lock l(all_hardware_lock);
  ck::vec<ck::ref<hw::Device>> all = all_hardware;

  return all;
}


void hw::Device::add_property(ck::string name, hw::Prop &&prop) {
  assert(!m_locked);
  m_props.set(name, move(prop));
}


ck::option<ck::string> hw::Device::get_prop_string(const ck::string &name) {
  if (m_props.contains(name)) {
    auto &prop = m_props.get(name);
    ck::string val = ck::string((char *)prop.data.data(), prop.data.size());
    return val;
  }
  return {};
}


ck::option<uint64_t> hw::Device::get_prop_int(const ck::string &name) {
  if (m_props.contains(name)) {
    auto &prop = m_props.get(name);
    uint64_t val = 0;
    if (prop.data.size() == 4) {
      val = *(uint32_t *)prop.data.data();
    } else if (prop.data.size() == 8) {
      val = *(uint64_t *)prop.data.data();
    } else {
      printk(KERN_WARN "prop '%s' could not be converted to an int of size 4 or 8\n", name.get());
    }
    return val;
  }
  return {};
}

bool hw::PCIDevice::is_device(uint16_t vendor, uint16_t device) const { return vendor_id == vendor && device_id == device; }



ck::vec<hw::Reg> hw::Prop::read_registers(void) const {
  ck::vec<hw::Reg> regs;


  ck::vec<uint64_t> vals;


  read_all_ints(vals, "as");
  if (address_cells == 0) {
    for (auto v : vals) {
      regs.push(hw::Reg{.address = 0, .size = v});
    }

  } else if (size_cells == 0) {
    for (auto v : vals) {
      regs.push(hw::Reg{.address = v, .size = 0});
    }
  } else {
    assert(vals.size() % 2 == 0);
    for (off_t i = 0; i < vals.size(); i += 2) {
      regs.push(hw::Reg{.address = vals[i], .size = vals[i + 1]});
    }
  }



  return regs;
}

ck::string hw::Prop::format(void) const {
  // return r;

  if (type == Prop::Type::String) {
    ck::string res;
    res += "\"";
    for (int i = 0; i < data.size() - 1; i++) {
      if (data[i] == '\0') {
        res += "\\0";
      } else {
        res += data[i];
      }
    }
    res += "\"";
    return res;
  }


  if (type == Prop::Type::CellSize) {
    uint64_t val = 0;
    ck::string res;
    off_t off = 0;

    read_cell(&val, 1, off);
    res += ck::string::format("<0x%lx>", val);
    return res;
  }

  if (type == Prop::Type::Integer) {
    uint64_t val = 0;
    ck::string res;
    off_t off = 0;

    read_size(&val, off);
    res += ck::string::format("<0x%lx>", val);
    return res;
  }



  if (type == Prop::Type::Register) {
    auto regs = read_registers();
    ck::string res;
    res += "<";
    int i = 0;
    for (auto reg : regs) {
      if (i != 0) res += " ";
      i++;
      res += ck::string::format("0x%lx 0x%lx", reg.address, reg.size);
    }
    res += ">";
    return res;
  }


  if (type == Prop::Type::List) {
    ck::vec<uint64_t> list;
    ck::string res;
    res += "<";
    read_all_ints(list, "s");
    for (int i = 0; i < list.size(); i++) {
      if (i != 0) res += " ";
      res += ck::string::format("%d", list[i]);
    }
    res += ">";
    return res;
  }



  if (type == Prop::Type::Interrupts) {
    ck::vec<uint64_t> list;
    ck::string res;
    res += "<";
    read_all_ints(list, "i");
    for (int i = 0; i < list.size(); i++) {
      if (i != 0) res += " ";
      res += ck::string::format("%d", list[i]);
    }
    res += ">";
    return res;
  }


  ck::string r;
  for (int i = 0; i < min(16, data.size()); i++) {
    r += ck::string::format("%02x ", data[i]);
  }
  if (data.size() > 16) r += "...";
  return r;
}


template <typename T>
static uint64_t bswap_cell(T *val, uint64_t *dst, int size_cell) {
  if (size_cell == 1) {
    *dst = __builtin_bswap32(*(uint32_t *)val);
    return 4;
  }
  if (size_cell == 2) {
    *dst = __builtin_bswap64(*(uint64_t *)val);
    return 8;
  }

  *dst = 0;
  return 0;
}


bool hw::Prop::read_cell(uint64_t *dst, int cells, off_t &off) const {
  if (cells == 0) {
    return true;
  }

  if (dst == NULL) return false;
  // sanity check
  if (data.size() < (off + cells * 4)) return false;


  off_t seek = bswap_cell(data.data() + off, dst, cells);
  if (seek == 0) return false;
  off += seek;
  return true;
}

bool hw::Prop::read_all_ints(ck::vec<uint64_t> &dst, const char *fmt) const {
  off_t off = 0;
  while (read_ints(dst, fmt, off)) {
  }

  return true;
}

bool hw::Prop::read_ints(ck::vec<uint64_t> &dst, const char *fmt, off_t &off) const {
  for (off_t i = 0; fmt[i] != 0; i++) {
    uint64_t val = 0;
    char c = fmt[i];

    switch (c) {
      case 'a':
        if (!read_address(&val, off)) return false;
        break;

      case 's':
        if (!read_size(&val, off)) return false;
        break;

      case 'i':
        if (!read_irq(&val, off)) return false;
        break;

      default:
        val = 0;
        panic("invalid format argument to read_ints");
        break;
    }

    dst.push(val);
  }
  return true;
}
