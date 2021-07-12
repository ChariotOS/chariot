#include <ck/json.h>


void ck::json::value::clear(void) {
  switch (m_type) {
    case Type::String:
      delete m_value.as_string;
      break;
    case Type::Object:
      delete m_value.as_object;
      break;
    case Type::Array:
      delete m_value.as_array;
      break;
    default:
      break;
  }
  m_type = Type::Null;
  m_value.as_string = nullptr;
}

bool ck::json::value::equals(const ck::json::value& other) const {
  if (other.type() != type()) return false;

  if (is_null() && other.is_null()) return true;

  if (is_bool() && other.is_bool() && as_bool() == other.as_bool()) return true;

  if (is_string() && other.is_string() && as_string() == other.as_string()) return true;

#if !defined(KERNEL)
  if (is_number() && other.is_number() && to_number<double>() == other.to_number<double>()) {
    return true;
  }
#else
  if (is_number() && other.is_number() && to_number<i64>() == other.to_number<i64>()) {
    return true;
  }
#endif

  if (is_array() && other.is_array() && as_array().size() == other.as_array().size()) {
    bool result = true;
    for (int i = 0; i < as_array().size(); ++i) {
      result &= as_array().at(i).equals(other.as_array().at(i));
    }
    return result;
  }

  if (is_object() && other.is_object() && as_object().size() == other.as_object().size()) {
    bool result = true;
    as_object().for_each_member(
        [&](auto& key, auto& value) { result &= value.equals(other.as_object().get(key)); });
    return result;
  }

  return false;
}

ck::string ck::json::value::format(void) {
  ck::string s;
  this->format(s);
  return s;
}

static void format_escaped_for_json(ck::string& dst, const ck::string& string) {
  for (auto ch : string) {
    switch (ch) {
      case '\e':
        dst += "\\u001B";
        break;
      case '\b':
        dst += "\\b";
        break;
      case '\n':
        dst += "\\n";
        break;
      case '\t':
        dst += "\\t";
        break;
      case '\"':
        dst += "\\\"";
        break;
      case '\\':
        dst += "\\\\";
        break;
      default:
        dst += ch;
    }
  }
}

void ck::json::value::format(ck::string& str) {
  switch (m_type) {
    case Type::String: {
      str += "\"";
      // TODO: string escape!
      format_escaped_for_json(str, as_string());
      str += "\"";
    } break;
    case Type::Array:
      m_value.as_array->format(str);
      break;
    case Type::Object:
      m_value.as_object->format(str);
      break;
    case Type::Bool:
      str += (m_value.as_bool ? "true" : "false");
      break;
#if !defined(KERNEL)
    case Type::Double:
      str += ck::string::format("%f", m_value.as_double);
      break;
#endif
    case Type::Int:
      str += ck::string::format("%lld", m_value.as_int);

      break;
    case Type::Uint:
      str += ck::string::format("%llu", m_value.as_int);
      break;
    case Type::Null:
      str += "null";
      break;
    default:
      panic("bad json input");
  }
}

void ck::json::object::format(ck::string& str) {
  str += "{";
  int i = 0;
  int count = m_members.size();
  for (auto& [key, value] : m_members) {
    str += "\"";
    format_escaped_for_json(str, key);
    str += "\": ";
    value.format(str);
    if (i != count - 1) str += ", ";
    i++;
  }
  str += "}";
}

void ck::json::array::format(ck::string& str) {
  str += "{";
  for (int i = 0; i < m_values.size(); i++) {
    m_values[i].format(str);
    if (i != m_values.size() - 1) str += ", ";
  }
  str += "}";
}