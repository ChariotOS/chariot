#include <mobo/dev_mgr.h>
#include <mobo/machine.h>
#include <stdio.h>
#include <sys/mman.h>

using namespace mobo;

static int init_devices();

mobo::machine::machine(driver &d) : m_driver(d) {
  init_devices();
  // init some machine state
}

mobo::machine::~machine(void) {
  // clean up machine state
  if (memory != nullptr) munmap(memory, mem_size);
}

void machine::allocate_ram(size_t nbytes) {
  // round up to the nearest page boundary
  if ((nbytes & 0xfff) != 0) {
    nbytes &= ~0xfff;
    nbytes += 0x1000;
  }

  memory = (char *)mmap(NULL, nbytes, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  mem_size = nbytes;

  // attach to the driver
  m_driver.attach_memory(mem_size, memory);
}

void machine::load_elf(std::string file) { return m_driver.load_elf(file); }

void machine::run() { return m_driver.run(); }

// setter for a machine
void driver::set_machine(machine *m) { m_machine = m; }


// This is a butchering of the section system, but it works
static int init_devices() {
  extern mobo::device_info __start__mobo_devices[];
  extern mobo::device_info __stop__mobo_devices[];
  mobo::device_info *dev = __start__mobo_devices;

  int i = 0;
  while (dev != __stop__mobo_devices) {
    if (dev->init != nullptr) {
      printf("Device %s with init at %p\n", dev->name, dev->init);
    }
    dev = &(__start__mobo_devices[++i]);
  }
  return 0;
}

int nothing_device_init(mobo::device_manager *man) { return 0; }

mobo_device_register("Generic", nothing_device_init)
