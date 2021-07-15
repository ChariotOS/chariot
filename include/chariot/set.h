#pragma once

#include <ck/map.h>

#ifndef __CHARIOT_SET_H
#define __CHARIOT_SET_H

template <typename T, typename KeyTraits = Traits<T>>
class set {
  struct entry_traits {
    static hash_t hash(const T& entry) { return KeyTraits::hash(entry); }
    static bool equals(const T& a, const T& b) { return KeyTraits::equals(a, b); }
  };

  typedef HashTable<T, entry_traits> HashTableType;
  typedef typename HashTableType::Iterator IteratorType;
  typedef typename HashTableType::ConstIterator ConstIteratorType;

 private:
 public:
  set() {}
  ~set() {}

  bool is_empty() const { return m_table.is_empty(); }
  int size() const { return m_table.size(); }
  int capacity() const { return m_table.capacity(); }
  void clear() { m_table.clear(); }

  void add(const T& v) { m_table.set(v); }
  bool contains(const T& key) const { return find(key) != end(); }

  void remove(const T& value) {
    auto it = find(value);
    if (it != end()) remove(it);
  }

  void remove(IteratorType it) { m_table.remove(it); }


  IteratorType begin() { return m_table.begin(); }
  IteratorType end() { return m_table.end(); }
  IteratorType find(const T& key) {
    return m_table.find(
        KeyTraits::hash(key), [&](auto& entry) { return KeyTraits::equals(key, entry); });
  }

  ConstIteratorType begin() const { return m_table.begin(); }
  ConstIteratorType end() const { return m_table.end(); }
  ConstIteratorType find(const T& key) const {
    return m_table.find(
        KeyTraits::hash(key), [&](auto& entry) { return KeyTraits::equals(key, entry); });
  }

 private:
  HashTableType m_table;
};

template <typename T>
set<T> operator&(const set<T>& a, const set<T>& b) {
  set<T> res;
  for (auto& v : a) {
    if (b.contains(v)) {
      res.add(v);
    }
  }
  return res;
}


#endif
