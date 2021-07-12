#pragma once

#include <ck/string.h>
#include <ck/ptr.h>
#include <ck/map.h>
#include <ck/option.h>
#include <ck/vec.h>

namespace ck {

  namespace json {

    enum class Type {
      Null,
      Int,
      Uint,
      Double,
      Bool,
      String,
      Array,
      Object,
    };

    class array;
    class object;



    class value {
     public:
      value(void) : m_type(ck::json::Type::Null) {}
#define JSON_VALUE_CTOR(type, json_type, member_name) \
  value(type v) : m_type(json_type) { m_value.as_##member_name = v; }
      JSON_VALUE_CTOR(uint64_t, ck::json::Type::Uint, uint);
      JSON_VALUE_CTOR(int64_t, ck::json::Type::Int, int);
      JSON_VALUE_CTOR(int, ck::json::Type::Int, int);
      JSON_VALUE_CTOR(long, ck::json::Type::Int, int);

      JSON_VALUE_CTOR(float, ck::json::Type::Double, double);
      JSON_VALUE_CTOR(double, ck::json::Type::Double, double);
      JSON_VALUE_CTOR(bool, ck::json::Type::Bool, bool);


      value(const char* str) : m_type(ck::json::Type::String) {
        m_value.as_string = new ck::string(str);
      }
      value(const ck::string& str) : m_type(ck::json::Type::String) {
        m_value.as_string = new ck::string(str);
      }

      value(ck::string&& str) : m_type(ck::json::Type::String) {
        m_value.as_string = new ck::string(move(str));
      }

      value(const json::array&);
      value(const json::object&);
      value(json::array&&);
      value(json::object&&);

      void clear();

      ~value(void) { clear(); }


#undef JSON_VALUE_CTOR


      ck::json::Type type(void) const { return m_type; }
      bool is_null(void) const { return type() == ck::json::Type::Null; }
      bool is_int(void) const { return type() == ck::json::Type::Int; }
      bool is_uint(void) const { return type() == ck::json::Type::Uint; }
      bool is_double(void) const { return type() == ck::json::Type::Double; }
      bool is_bool(void) const { return type() == ck::json::Type::Bool; }
      bool is_string(void) const { return type() == ck::json::Type::String; }
      bool is_array(void) const { return type() == ck::json::Type::Array; }
      bool is_object(void) const { return type() == ck::json::Type::Object; }


      // aux
      bool is_number(void) const { return is_int() || is_uint() || is_double(); }

      template <typename T>
      T to_number(T default_value = 0) const {
#if !defined(KERNEL)
        if (is_double()) return (T)as_double();
#endif
        if (type() == Type::Int) return (T)as_int();
        if (type() == Type::Uint) return (T)as_uint();
        return default_value;
      }

      bool equals(const ck::json::value& other) const;


      int64_t as_int() const {
        assert(is_int());
        return m_value.as_int;
      }

      uint64_t as_uint() const {
        assert(is_uint());
        return m_value.as_uint;
      }

      int as_bool() const {
        assert(is_bool());
        return m_value.as_bool;
      }

      ck::string as_string() const {
        assert(is_string());
        return *m_value.as_string;
      }

      const ck::json::object& as_object() const {
        assert(is_object());
        return *m_value.as_object;
      }

      const ck::json::array& as_array() const {
        assert(is_array());
        return *m_value.as_array;
      }

#if !defined(KERNEL)
      double as_double() const {
        assert(is_double());
        return m_value.as_double;
      }
#endif

      ck::string format(void);
      void format(ck::string& str);


     private:
      ck::json::Type m_type = ck::json::Type::Null;

      union {
        ck::string* as_string = nullptr;
        ck::json::array* as_array;
        ck::json::object* as_object;
#if !defined(KERNEL)
        double as_double;
#endif
        int64_t as_int;
        uint64_t as_uint;
        bool as_bool;
      } m_value;
    };



    class array {
     public:
      array(const array& other) : m_values(other.m_values) {}

      array(array&& other) : m_values(move(other.m_values)) {}

      template <typename T>
      array(const ck::vec<T>& vector) {
        for (auto& value : vector)
          m_values.push(move(value));
      }

      void format(ck::string& str);


      size_t size(void) const { return m_values.size(); }
      const ck::json::value& at(size_t index) const { return m_values.at(index); }
      const ck::json::value& operator[](size_t index) const { return m_values.at(index); }

     private:
      ck::vec<ck::json::value> m_values;
    };

    class object {
     public:
      void format(ck::string& str);

      template <typename Fn>
      inline void for_each_member(Fn cb) const {
        for (auto& [key, value] : m_members) {
          cb(key, value);
        }
      }


      ck::json::value get(const ck::string& key) const {
        auto* value = get_ptr(key);
        return value ? *value : ck::json::value();
      }

      ck::json::value get_or(const ck::string& key, ck::json::value& default_value) const {
        auto* value = get_ptr(key);
        if (value) return *value;
        return default_value;
      }

      const ck::json::value* get_ptr(const ck::string& key) const {
        auto it = m_members.find(key);
        if (it == m_members.end()) {
          return NULL;
        }
        return &(*it).value;
      }

      bool has(const ck::string& key) const { return m_members.contains(key); }
      void set(const ck::string& key, ck::json::value val) { m_members.set(key, val); }
      void del(const ck::string& key) { m_members.remove(key); }

      size_t size(void) const { return m_members.size(); }

     private:
      //   ck::vec<ck::string> m_order;
      ck::map<ck::string, ck::json::value> m_members;
    };


  };  // namespace json


  using json_val = ck::json::value;

}  // namespace ck