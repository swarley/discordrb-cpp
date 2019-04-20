#pragma once

#include "nlohmann/json.hpp"

#include <rice/Hash.hpp>

namespace Discordrb {
using json = nlohmann::json;

namespace JSONConverter {
Rice::Array array_from_json(json& json_data);
Rice::Hash hash_from_json(json& json_data);
}  // namespace JSONConverter
}  // namespace Discordrb