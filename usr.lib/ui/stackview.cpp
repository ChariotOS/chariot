#include <math.h>
#include <ui/view.h>



ui::stackview::~stackview() {}


void ui::stackview::reflow_impl() {
  //
  // get the main and cross axis. For example, if the layout is vertical, this
  // is the result:
  //       +------+------+
  //       |      |      |
  //       |      |< main axis
  //       |      |      |
  //       | -----|------|
  //       |      |   ^  |
  //       |      |   Cross axis
  //       +------+------+
  //                        cross axis
  //
  auto main_axis = m_layout;
  auto cross_axis = main_axis == ui::direction::horizontal
                        ? ui::direction::vertical
                        : ui::direction::horizontal;


  // auto available_size = gfx::point(width(), height());

  int nfixed = 0;
  int ncalc = 0;

  // how much space is avail on the main axis
  size_t available_size = this->size(main_axis);
  // how much space is used by fixed sized entries
  size_t fixed_space_used = 0;


  ui::view *last_child = NULL;
  each_child(fn(ui::view & entry) {
    last_child = &entry;
    switch (entry.get_size_policy(main_axis)) {
      case ui::size_policy::Calculate:
        ncalc++;
        break;
      case ui::size_policy::Fixed:
        // this element is a fixed size, so we need to
        // adjust the calculated size of other entries from it
        fixed_space_used += entry.size(main_axis);
        nfixed++;
        break;
    }
  });

  // equally divide the remaining size among the calculated components
  float equal_stretch_size =
      (available_size - fixed_space_used) / (float)(ncalc == 0 ? 1 : ncalc);

	// the current position in the layout
  size_t position = 0;

  each_child(fn(ui::view & entry) {
    entry.set_pos(cross_axis, 0);
    // printf("pos: %d\n", position);
    entry.set_pos(main_axis, position);
    // TODO: paddings
    entry.set_size(cross_axis, size(cross_axis));
    switch (entry.get_size_policy(main_axis)) {
      case ui::size_policy::Calculate: {
        int size = equal_stretch_size;
        if (&entry == last_child) {
          float round_error = equal_stretch_size - (float)size;
          if (round_error > 0) size++;
        }
        entry.set_size(main_axis, size);
        position += equal_stretch_size;
        break;
      }
      case ui::size_policy::Fixed: {
        position += entry.size(main_axis);
        break;
      }
    }
  });
}
