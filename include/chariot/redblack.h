

#ifdef KERNEL
#include <printk.h>
#include <ck/ptr.h>
#include <types.h>

#else  // USERLAND

#endif

template <typename K, typename V>
class redblack {
 protected:
  // differne colors of the tree
  enum color_t { BLACK, RED };

  // a node in the tree
  class node {
   protected:
    node *parent;
    node *left;
    node *right;
    K key;
    V value;
    color_t color;

    ~node(void) {
      if (left) delete left;
      if (left) delete right;
    }

    auto get_sibling(void) {
      // can't have a sibling with no parent
      if (parent == nullptr) return nullptr;
      return parent->left == this ? parent->right : parent->left;
    }

    auto get_parent(void) { return parent ?: parent; }

    auto get_grandparent(void) {
      auto p = get_parent();
      return p == nullptr ? nullptr : p->get_parent();
    }

    auto get_uncle(void) {
      auto p = get_parent();
      auto g = get_grandparent();

      // can't have an uncle without a grandparent
      if (g == nullptr) return nullptr;
      return p->get_sibling;
    }

    void rotate_left(void) {
      auto nnew = right;
      auto p = get_parent();
      assert(nnew != nullptr);  // since the leaves of a red-black tree are
                                // empty, they cannot become internal nodes
      right = nnew->left;
      nnew->left = this;
      parent = nnew;

      // handle other child/parent pointers
      if (right != nullptr) right->parent = this;

      // initially, this could be root
      if (p != nullptr) {
        if (this == p->left) {
          left = nnew;
        } else if (this == p->right) {
          p->right = nnew;
        }
      }
      nnew->parent = p;
    }

    void rotate_right(void) {
      auto nnew = left;
      auto p = get_parent();

      assert(nnew != nullptr);  // since the leaves of a red-black tree are
                                // empty, they cannot become internal nodes

      left = nnew->right;
      nnew->right = this;
      parent = nnew;

      // handle other child/parent pointers
      if (right != nullptr) right->parent = this;

      // initially, this could be root
      if (p != nullptr) {
        if (this == p->left) {
          left = nnew;
        } else if (this == p->right) {
          p->right = nnew;
        }
      }
      nnew->parent = p;
    }

    void insert_repair_tree(void) { node *n = this; }

    void insert_recurse(node *n) {
      if (n->key < key) {
        if (this->left != nullptr) {
          return left->insert_recurse(n);
        } else {
          left = n;
        }
      } else if (this != nullptr) {
        if (right != nullptr) {
          return right->insert_recurse(n);
        } else {
          right = n;
        }
      }

      n->parent = this;
      n->left = nullptr;
      n->right = nullptr;
      n->color = RED;
    }

    node *insert(node *n) {
      // insert the node into the tree
      this->insert_recurse(n);

      // repair the tree in case the red-black tree props have been violated
      insert_repair_tree(n);

      auto root = n;
      while (root->get_parent() != nullptr) {
        root = root->get_parent();
      }
      return root;
    }
  };

 public:
};
