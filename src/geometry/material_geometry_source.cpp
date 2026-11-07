#include "material_geometry_source.hpp"

#include <nlohmann/json.hpp>

namespace blcad::geometry::detail {
Result<std::string> material_independent_geometry_source(std::string_view source) {
  try {
    auto value = nlohmann::json::parse(source);
    if (value.contains("history_version")) {
      value["version"] = value.at("history_version");
      value.erase("history_version");
    }
    value.erase("material_assignments");
    if (value.contains("engineering_resources")) {
      auto& entries = value.at("engineering_resources").at("entries");
      for (auto it = entries.begin(); it != entries.end();)
        if (it->at("kind") == "material")
          it = entries.erase(it);
        else
          ++it;
      if (entries.empty())
        value.erase("engineering_resources");
    }
    return Result<std::string>::success(value.dump());
  } catch (const std::exception& e) {
    return Result<std::string>::failure(Error::validation("physical.geometry_source", e.what()));
  }
}
} // namespace blcad::geometry::detail
