#include <fs.h>



fs::DeviceNode::DeviceNode(void) : fs::Node(nullptr) {}


static spinlock names_lock;
static ck::map<ck::string, ck::box<fs::DirectoryEntry>> names;


fs::DeviceNode::~DeviceNode(void) { unbind(); }

void fs::DeviceNode::bind(ck::string name) {
  m_name = name;

  auto l = fs::DeviceNode::lock_names();

  if (names.contains(name)) {
    KWARN("fs::DeviceNode::bind: '%s' exists\n", name.get());
    return;
  }

  names[name] = ck::make_box<fs::DirectoryEntry>(name, this);
}


void fs::DeviceNode::unbind(void) {
  if (m_name == "") return;
  auto l = fs::DeviceNode::lock_names();

  if (!names.contains(m_name)) {
    KWARN("fs::DeviceNode::unbind: '%s' exists\n", m_name.get());
    return;
  }
  m_name = "";
  names.remove(m_name);
}


scoped_irqlock fs::DeviceNode::lock_names(void) { return names_lock; }

ck::map<ck::string, ck::box<fs::DirectoryEntry>> &fs::DeviceNode::get_names(void) { return names; }
