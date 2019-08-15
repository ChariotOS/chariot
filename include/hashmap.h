#pragma once

#include <template_lib.h>

#define HASHMAP_SIZE 8

template <typename K, typename V, typename KeyTraits = Traits<K>>
class hashmap {
  struct bucket {
    K key;
    V value;
  };

  u64 m_size = 0;

  bucket **m_buckets = nullptr;

  bucket *buck(const K &key) { u64 hash = KeyTraits::hash(key); }

 public:
  hashmap() {
      m_buckets = new bucket *[HASHMAP_SIZE];
      for (int i = 0; i < HASHMAP_SIZE; i++) {
        m_buckets[i] = nullptr;
      }
  }

  ~hashmap() {
    if (m_buckets != nullptr) {
      for (int i = 0; i < HASHMAP_SIZE; i++) {
        bucket *b = m_buckets[i];
        while (b != nullptr) {
          bucket *n = b->next;
          delete b;
          b = n;
        }
      }

      delete[] m_buckets;
    }
  }

  bool empty() const { return m_size == 0; }

  bool contains(const K &);

  // void set(const K& key, 
};
