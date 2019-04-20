#include "discordrb/json_converters.hpp"
#include <iostream>

namespace Discordrb {
using json = nlohmann::json;
using jvalue = json::value_t;

namespace JSONConverter {

Rice::Array array_from_json(json& json_data) {
  Rice::Array arr;
  jvalue type;

  for (auto it : json_data.items()) {
    type = it.value().type();

    if (type == jvalue::string) {
      arr.push(static_cast<std::string>(it.value()));
    } else if (type == jvalue::number_integer ||
               type == jvalue::number_unsigned) {
      arr.push(static_cast<int>(it.value()));
    } else if (type == jvalue::number_float) {
      arr.push(static_cast<float>(it.value()));
    } else if (type == jvalue::boolean) {
      arr.push(static_cast<bool>(it.value()));
    } else if (type == jvalue::array) {
      arr.push(array_from_json(it.value()));
    } else if (type == jvalue::object) {
      arr.push(hash_from_json(it.value()));
    }
  }

  return arr;
}

Rice::Hash hash_from_json(json& json_data) {
  Rice::Hash hash;
  jvalue type;
  std::string key;

  for (auto it : json_data.items()) {
    type = it.value().type();
    key = static_cast<std::string>(it.key());

    if (type == jvalue::string) {
      hash[key] = static_cast<std::string>(it.value());
    } else if (type == jvalue::number_integer ||
               type == jvalue::number_unsigned) {
      hash[key] = static_cast<int>(it.value());
    } else if (type == jvalue::number_float) {
      hash[key] = static_cast<float>(it.value());
    } else if (type == jvalue::boolean) {
      hash[key] = static_cast<bool>(it.value());
    } else if (type == jvalue::array) {
      hash[key] = array_from_json(it.value());
    } else if (type == jvalue::object) {
      hash[key] = hash_from_json(it.value());
    }
  }

  return hash;
}
}  // namespace JSONConverter
}  // namespace Discordrb
