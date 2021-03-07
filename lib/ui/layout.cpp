#include <ui/layout.h>



ck::string ui::layout::node::attr(ui::attr a) {
  if (m_attrs.contains(a)) {
    return m_attrs[a];
  }
  return {};
}

long ui::layout::node::attr_num(ui::attr a) {
  if (m_attrs.contains(a)) {
    long n = 0;
    if (sscanf(m_attrs[a].get(), "%ld", &n) == 0) {
      ck::err.fmt("ui::layout::node attr '%s' is an invalid integer\n", m_attrs[a].get());
      return LAYOUT_INT_ERR;
    }
    return n;
  }
  return LAYOUT_INT_ERR;
}


void ui::layout::node::set_attr(ui::attr a, ck::string val) {
  m_attrs[a] = val;
}
