#include <dev/driver.h>
#include <printf.h>

// dev::Device::Device(void) : fs::Node(nullptr) {}
dev::Device::Device(dev::Driver &driver, ck::ref<hw::Device> dev) : fs::Node(nullptr), m_driver(driver), m_dev(dev) {
  if (m_dev) m_dev->attach_to(this);
}
static spinlock names_lock;
static ck::map<ck::string, ck::box<fs::DirectoryEntry>> names;


dev::Device::~Device(void) {
	printf("device node destructor!\n");
  if (m_dev != nullptr) m_dev->detach();
  unbind();
}

void dev::Device::bind(ck::string name) {
  m_name = name;

  auto l = dev::Device::lock_names();

  if (names.contains(name)) {
    KWARN("dev::Device::bind: '%s' exists\n", name.get());
    return;
  }

  names[name] = ck::make_box<fs::DirectoryEntry>(name, this);
}


void dev::Device::unbind(void) {
  if (m_name == "") return;
  auto l = dev::Device::lock_names();

  if (!names.contains(m_name)) {
    KWARN("dev::Device::unbind: '%s' exists\n", m_name.get());
    return;
  }
  m_name = "";
  names.remove(m_name);
}


scoped_irqlock dev::Device::lock_names(void) { return names_lock; }

ck::map<ck::string, ck::box<fs::DirectoryEntry>> &dev::Device::get_names(void) { return names; }



ssize_t dev::BlockDevice::read(fs::File &f, char *dst, size_t bytes) { return bread(*this, (void *)dst, bytes, f.offset()); }
ssize_t dev::BlockDevice::write(fs::File &f, const char *dst, size_t bytes) { return bwrite(*this, (void *)dst, bytes, f.offset()); }
