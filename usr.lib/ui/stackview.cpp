#include <math.h>
#include <ui/view.h>


/*

ui::stackview::~stackview() {}


void ui::stackview::reflow_impl() {
  //
  // get the main and cross axis. For example, if the layout is Vertical, this
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
  auto cross_axis = main_axis == ui::Direction::Horizontal
                        ? ui::Direction::Vertical
                        : ui::Direction::Horizontal;


  // auto available_size = gfx::point(width(), height());

  int nfixed = 0;
  int ncalc = 0;

  // how much space is avail on the main axis
  size_t available_size = this->size(main_axis) - padding.total_for(main_axis) - (bordersize * 2);
  // how much space is used by fixed sized entries
  size_t fixed_space_used = 0;


  ui::view *last_child = NULL;
  each_child(fn(ui::view & entry) {
    last_child = &entry;
    switch (entry.get_size_policy(main_axis)) {
      case ui::SizePolicy::Calculate:
        ncalc++;
        break;
      case ui::SizePolicy::Fixed:
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
  size_t position = padding.base_for(main_axis) + bordersize;

  each_child(fn(ui::view & entry) {
    entry.set_pos(cross_axis, padding.base_for(cross_axis) + bordersize);
    // printf("pos: %d\n", position);
    entry.set_pos(main_axis, position);
    // TODO: paddings
    entry.set_size(cross_axis, size(cross_axis) - padding.total_for(cross_axis) - bordersize * 2);
    switch (entry.get_size_policy(main_axis)) {
      case ui::SizePolicy::Calculate: {
        int size = equal_stretch_size;
        if (&entry == last_child) {
          float round_error = equal_stretch_size - (float)size;
          if (round_error > 0) size++;
        }
        entry.set_size(main_axis, size);
        position += equal_stretch_size;
        break;
      }
      case ui::SizePolicy::Fixed: {
        position += entry.size(main_axis);
        break;
      }
    }
  });
}
*/
