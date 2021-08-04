#include <ui/application.h>
#include <gfx/geom.h>
#include <ui/boxlayout.h>
#include "sys/sysbind.h"
#include "ui/event.h"
#include "ui/textalign.h"
#include <ck/timer.h>
#include <ui/frame.h>
#include <ck/pair.h>
#include <ui/label.h>
#include <ck/json.h>
#include <ck/time.h>



class ct_window : public ui::window {
 public:
  ct_window(void) : ui::window("current test", 400, 400) {}

  virtual ck::ref<ui::view> build(void) {
    // clang-format off

    ui::style st = {
      .background = 0xE0E0E0,
      .margins = 10,
      .padding = 10,
    };

    return ui::make<ui::vbox>({
      ui::make<ui::hbox>(st, {
          ui::make<ui::label>("Top Left", ui::TextAlign::TopLeft),
          ui::make<ui::label>("Top Center", ui::TextAlign::TopCenter),
          ui::make<ui::label>("Top Right", ui::TextAlign::TopRight),
        }),

      ui::make<ui::hbox>(st, {
          ui::make<ui::label>("Left", ui::TextAlign::CenterLeft),
          ui::make<ui::label>("Center", ui::TextAlign::Center),
          ui::make<ui::label>("Right", ui::TextAlign::CenterRight),
        }),

      
      ui::make<ui::hbox>(st, {
          ui::make<ui::label>("Bottom Left", ui::TextAlign::BottomLeft),
          ui::make<ui::label>("Bottom Center", ui::TextAlign::BottomCenter),
          ui::make<ui::label>("Bottom Right", ui::TextAlign::BottomRight),
        }),
  
    });

    // clang-format on
  }
};



#include <chariot/rbtree.h>
#include <ck/option.h>

namespace ck {

  // a redblack tree based key value store.
  // NOT THREAD SAFE.
  template <class K, class V, typename KeyTraits = Traits<K>>
  class newmap {
    using hash_t = unsigned long;
    struct map_entry {
      // both the key and the value must have a default constructor (and a copy constructor)
      K key = {};
      V value = {};
    };

    // what exists in the tree
    struct tree_node {
      rb_node node;
      hash_t hash;
      map_entry data;
    };

   public:
    newmap(void) { m_tree.rb_node = NULL; }

    ~newmap(void) {
      tree_node *n, *node;
      rbtree_postorder_for_each_entry_safe(node, n, &m_tree, node) { delete node; }
    }


    // ::set a key in the map to a value, replacing any existing mapping
    // that previously existed. If the key is not in the map, it is added
    // to a new node.
    void set(const K& key, const V& value) { set(map_entry{key, value}); }
    void set(const K& key, V&& value) { set(map_entry{key, move(value)}); }



    // ::get fetches a *copy* of the value in located at the given key. To
    // get a reference, use ::at or ::operator[], which will cause errors
    // if the key is not in the map.
    ck::option<V> get(const K& key) const {
      hash_t hash = KeyTraits::hash(key);

      auto* node = lookup(key, hash);
      if (node != NULL) {
        return node->data.value;
      }

      return {};
    }

    const V& at(const K& key) const {
      hash_t hash = KeyTraits::hash(key);
      auto* node = lookup(key, hash);
      if (node == NULL) {
        panic("Key does not exist");
      }
      return node->data.value;
    }

    V& at(const K& key) {
      hash_t hash = KeyTraits::hash(key);
      auto* node = lookup(key, hash);
      if (node == NULL) {
        panic("Key does not exist");
      }
      return node->data.value;
    }

   private:
    void set(map_entry&& entry) {
      hash_t hash = KeyTraits::hash(entry.key);
      auto* ent = lookup(entry.key, hash);
      // if the node was already in the map, simply replace the value
      if (ent != NULL) {
        ent->data.value = entry.value;
        return;
      }

      // otherwise, create a new node add it to the tree
      tree_node* node = new tree_node;
      RB_CLEAR_NODE(&node->node);
      node->hash = hash;
      node->data = move(entry);

      add_tree_node(node);
    }


    // IMPORTANT: the key must be unique and not currently in the tree when you call this function
    void add_tree_node(tree_node* node) {
      // insert the node into the tree
      rb_insert(m_tree, &node->node, [&](struct rb_node* other) {
        auto* other_ent = rb_entry(other, struct tree_node, node);

        if (other_ent->hash < node->hash) return RB_INSERT_GO_LEFT;
        return RB_INSERT_GO_RIGHT;
      });
    }


    // Lookup the node for a given hash in the tree,
    tree_node* lookup(const K& key, hash_t hash) const {
      struct rb_node* const* n = &(m_tree.rb_node);
      struct rb_node* parent = NULL;

      int steps = 0;

      /* Figure out where to put new node */
      while (*n != NULL) {
        auto* r = rb_entry(*n, struct tree_node, node);

        parent = *n;
        steps++;

        if (r->hash == hash) {
          // compare the key to avoid cache colisions
          if (r->data.key == key) {
            return r;
          }
        }

        if (r->hash < hash) {
          n = &((*n)->rb_left);
        } else if (r->hash >= hash) {
          // equal hash goes right
          n = &((*n)->rb_right);
        }
      }
      return NULL;
    }

    rb_root m_tree;
  };
}  // namespace ck


template <class M>
void test_map(const char* name) {
  for (long entries = 0; entries < 3'000'000; entries += 250'000) {
    M m;

    auto start = ck::time::us();
    for (int i = 0; i < entries; i++)
      m.set(i, i);
    auto end = ck::time::us();

    printf("%s, %llu, %llu\n", name, entries, (end - start));
  }
}


int main(int argc, char** argv) {
  // test_map<ck::newmap<int, int>>("ck::newmap");
  // test_map<ck::map<int, int>>("ck::map");
  // sysbind_shutdown();

  ui::application app;
  ct_window* win = new ct_window;

  app.start();
  return 0;
}
