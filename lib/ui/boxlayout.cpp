#include <ui/boxlayout.h>
#include <ui/view.h>
#include <math.h>

gfx::isize ui::boxlayout::preferred_size() const {
  gfx::isize size;
  size.set_primary_size_for_direction(direction(), preferred_primary_size());
  size.set_secondary_size_for_direction(direction(), preferred_secondary_size());
  return size;
}

int ui::boxlayout::preferred_primary_size() const {
  int size = 0;

  for (auto& child : m_owner->m_children) {
    // TODO: is_visible()
    int min_size = child->min_size().primary_size_for_direction(direction());
    int max_size = child->max_size().primary_size_for_direction(direction());
    int preferred_primary_size = -1;

    if (child->is_shrink_to_fit() && child->layout()) {
      preferred_primary_size =
          child->layout()->preferred_size().primary_size_for_direction(direction());
    }
    int item_size = MAX(0, preferred_primary_size);
    item_size = MAX(min_size, item_size);
    item_size = MIN(max_size, item_size);
    size += item_size + spacing();
  }
  if (size > 0) size -= spacing();

  size += padding().total_for(direction());

  if (!size) return -1;
  return size;
}

int ui::boxlayout::preferred_secondary_size() const {
  int size = 0;
  for (auto& entry : m_owner->m_children) {
    // TODO: is_visible()
    int min_size = entry->min_size().secondary_size_for_direction(direction());
    int preferred_secondary_size = -1;
    if (entry->is_shrink_to_fit() && entry->layout()) {
      preferred_secondary_size =
          entry->layout()->preferred_size().secondary_size_for_direction(direction());
      size = MAX(size, preferred_secondary_size);
    }
    size = MAX(min_size, size);
  }

  size += padding().total_for(direction());

  if (!size) return -1;
  return size;
}


void ui::boxlayout::run(ui::view& view) {
  // printf("run box layout!\n");
  if (m_owner == NULL) return;
  if (m_owner->m_children.is_empty()) return;

  // printf("m_owner!\n");
  struct Item {
    ui::view* view{nullptr};
    int min_size{-1};
    int max_size{-1};
    int size{0};
    bool final{false};
  };

  ck::vec<Item> items;
  items.ensure_capacity(m_owner->m_children.size());

  auto& ents = m_owner->m_children;

  for (size_t i = 0; i < ents.size(); i++) {
    auto& entry = ents[i];

    // TODO: is_visible();
    auto min_size = entry->min_size();
    auto max_size = entry->max_size();

    if (entry->is_shrink_to_fit() && entry->layout()) {
      auto preferred_size = entry->layout()->preferred_size();
      min_size = max_size = preferred_size;
    }

    int m = entry->margins().total_for(direction());

    items.push(Item{entry.get(), min_size.primary_size_for_direction(direction()),
                    max_size.primary_size_for_direction(direction())});
  }

  if (items.is_empty()) return;
  int available_size =
      view.size().primary_size_for_direction(direction()) - spacing() * (items.size() - 1);
  int unfinished_items = items.size();


  available_size -= padding().total_for(direction());


  // Pass 1: Set all items to their minimum size.
  for (auto& item : items) {
    item.size = 0;
    if (item.min_size >= 0) item.size = item.min_size;
    available_size -= item.size;
    available_size -= item.view->margins().total_for(direction());

    if (item.min_size >= 0 && item.max_size >= 0 && item.min_size == item.max_size) {
      // Fixed-size items finish immediately in the first pass.
      item.final = true;
      --unfinished_items;
    }
  }

  // Pass 2: Distribute remaining available size evenly, respecting each item's maximum size.
  while (unfinished_items && available_size > 0) {
    int slice = available_size / unfinished_items;
    // If available_size does not divide evenly by unfinished_items,
    // there are some extra pixels that have to be distributed.
    int pixels = available_size - slice * unfinished_items;
    available_size = 0;

    for (auto& item : items) {
      if (item.final) continue;

      int pixel = pixels ? 1 : 0;
      pixels -= pixel;
      int item_size_with_full_slice = item.size + slice + pixel;
      item.size = item_size_with_full_slice;

      if (item.max_size >= 0) item.size = MIN(item.max_size, item_size_with_full_slice);

      // If the slice was more than we needed, return remained to available_size.
      int remainder_to_give_back = item_size_with_full_slice - item.size;
      available_size += remainder_to_give_back;

      if (item.max_size >= 0 && item.size == item.max_size) {
        // We've hit the item's max size. Don't give it any more space.
        item.final = true;
        --unfinished_items;
      }
    }
  }

  // Pass 3: Place the views.
  int current_x = padding().left;
  int current_y = padding().top;

  auto view_rect_with_padding_removed = view.rect();
  view_rect_with_padding_removed.take_from_left(padding().left);
  view_rect_with_padding_removed.take_from_top(padding().top);
  view_rect_with_padding_removed.take_from_right(padding().right);
  view_rect_with_padding_removed.take_from_bottom(padding().bottom);

  for (auto& item : items) {
    gfx::rect rect{current_x, current_y, 0, 0};
    // printf("current x:%d y:%d\n", current_x, current_y);

    ui::edges margins = item.view->margins();

    int primary_margin = margins.total_for(direction());

    rect.set_primary_size_for_direction(direction(), item.size);

    if (item.view) {
      int secondary = view.size().secondary_size_for_direction(direction());

      secondary -= padding().total_for_secondary(direction());

      secondary -= margins.total_for_secondary(direction());

      int min_secondary = item.view->min_size().secondary_size_for_direction(direction());
      int max_secondary = item.view->max_size().secondary_size_for_direction(direction());
      if (min_secondary >= 0) secondary = MAX(secondary, min_secondary);
      if (max_secondary >= 0) secondary = MIN(secondary, max_secondary);

      gfx::rect view_rect_with_margins_removed = view_rect_with_padding_removed;


      view_rect_with_margins_removed.take_from_left(margins.left);
      view_rect_with_margins_removed.take_from_top(margins.top);
      view_rect_with_margins_removed.take_from_right(margins.right);
      view_rect_with_margins_removed.take_from_bottom(margins.bottom);

      rect.set_secondary_size_for_direction(direction(), secondary);

      if (direction() == gfx::Direction::Horizontal) {
        rect.x += margins.left;
      } else {
        rect.y += margins.top;
      }

      if (direction() == gfx::Direction::Horizontal) {
        rect.center_vertically_within(view_rect_with_margins_removed);

      } else {
        rect.center_horizontally_within(view_rect_with_margins_removed);
      }

      item.view->set_relative_rect(rect);
    }

    if (direction() == gfx::Direction::Horizontal) {
      current_x += rect.w + spacing() + primary_margin;
    } else {
      current_y += rect.h + spacing() + primary_margin;
    }
  }
}
