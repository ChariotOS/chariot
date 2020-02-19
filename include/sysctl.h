#pragma once

#ifndef __SYSCTL_H
#define __SYSCTL_H

#include <errno.h>
#include <lock.h>
#include <map.h>
#include <ptr.h>
#include <string.h>

namespace sys {

#define ST_UNKNOWN 0
#define ST_STRING 1
#define ST_MAP 2

class value : public refcounted<sys::value> {
 public:
  virtual ~value(void);
  virtual string str(void) = 0;
  virtual int type(void) = 0;

  virtual ref<sys::value> get(const string &key) { return nullptr; }
  virtual void set(const string &key, ref<sys::value> val) { /* nop */
  }
  virtual int rem(const string &key) { return -EINVAL; }
};

class map_value final : public sys::value {
  spinlock m_lock;
  map<string, ref<sys::value>> m_map;

 public:
  inline virtual ~map_value(void);
  virtual string str(void);
  inline int type(void) { return ST_MAP; }

  virtual void set(const string &key, ref<sys::value> val);
  virtual int rem(const string &key);
};

class string_value final : public sys::value {
  string m_val;

 public:

  inline string_value(const char *v) : m_val(v) {}
  inline string_value(const string &v) : m_val(v) {}

  inline virtual ~string_value(void);
  virtual inline string str(void) { return m_val; }
  inline int type(void) { return ST_STRING; }
};

string get_key(const char *);

}  // namespace sys

#endif

