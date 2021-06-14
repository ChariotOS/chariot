#include <ui/layout.h>
#include <ui/view.h>

ui::layout::~layout(void) {}


void ui::layout::notify_adopted(ui::view &v) { m_owner = &v; }

void ui::layout::notify_disowned(ui::view &v) {
  assert(&v == m_owner);
  m_owner = nullptr;
}


void ui::layout::set_spacing(int spacing) {
  if (m_spacing == spacing) return;
  m_spacing = spacing;
}


const ui::edges &ui::layout::margins() const {
  assert(m_owner);
  return m_owner->margins();
}

const ui::edges &ui::layout::padding() const {
  assert(m_owner);
  return m_owner->padding();
}