#include <sysctl.h>

string sys::map_value::str(void) {

  m_lock.lock();
  string s;
  s += "{";

  int i = 0;

  for (auto &kv : m_map) {
    s += kv.key;
    s += ":";
    s += kv.value->str();;

    if (i != m_map.size() - 1) {
      s += ",";
    }
  }

  s += "}";

  m_lock.unlock();
  return s;
}

sys::value::~value(void) {}
sys::map_value::~map_value(void) {}
sys::string_value::~string_value(void) {}


void sys::map_value::set(const string &key, ref<sys::value> val) {
  assert(val.get() != NULL);
  m_lock.lock();
  m_map.set(key, val);
  m_lock.unlock();
}

int sys::map_value::rem(const string &key) {
  m_lock.lock();
  m_map.remove(key);
  m_lock.unlock();
  return 0;
}
