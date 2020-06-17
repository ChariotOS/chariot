#include <ck/object.h>
#include <stdio.h>



ck::intrusive_list<ck::object, &ck::object::m_all_list>&
ck::object::all_objects() {
  static ck::intrusive_list<ck::object, &ck::object::m_all_list> objects;
  return objects;
}


ck::object::object(void) {
  all_objects().append(*this);

}
ck::object::~object(void) {
  all_objects().remove(*this);
}
