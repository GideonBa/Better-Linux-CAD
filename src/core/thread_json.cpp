#include "thread_json.hpp"
#include "engineering_resource_json.hpp"

#include <exception>
#include <nlohmann/json.hpp>
#include <utility>

namespace blcad::detail {
namespace {
using json = nlohmann::json;
bool exact(const json& value, std::initializer_list<const char*> fields) {
  if (!value.is_object() || value.size() != fields.size())
    return false;
  for (const auto* field : fields)
    if (!value.contains(field))
      return false;
  return true;
}
} // namespace

nlohmann::json thread_definition_to_json(const ThreadDefinition& definition) {
  json profile;
  if (const auto* p = std::get_if<ThreadParameterProfile>(&definition.profile())) {
    profile = {{"kind", "custom"},
               {"system", p->system},
               {"designation", p->designation},
               {"tolerance", p->tolerance},
               {"major_diameter", p->major_diameter.value()},
               {"minor_diameter", p->minor_diameter.value()},
               {"pitch", p->pitch.value()},
               {"included_angle", p->included_angle.value()}};
    if (p->taper_angle)
      profile["taper_angle"] = p->taper_angle->value();
  } else {
    const auto& selected = std::get<ThreadStandardProfile>(definition.profile());
    const auto& c = selected.columns;
    json columns{{"system", c.system},
                 {"designation", c.designation},
                 {"tolerance", c.tolerance},
                 {"major_diameter", c.major_diameter},
                 {"minor_diameter", c.minor_diameter},
                 {"pitch", c.pitch},
                 {"included_angle", c.included_angle}};
    if (c.taper_angle)
      columns["taper_angle"] = *c.taper_angle;
    profile = {{"kind", "standard"},
               {"table", engineering_resource_key_to_json(selected.table)},
               {"row", selected.row},
               {"columns", std::move(columns)}};
  }
  json result{{"profile", std::move(profile)},
              {"length", definition.length().value()},
              {"handedness", to_string(definition.handedness())},
              {"representation", to_string(definition.representation())}};
  if (definition.start())
    result["start"] = definition.start()->value();
  return result;
}

Result<ThreadDefinition> thread_definition_from_json(const nlohmann::json& value) {
  const auto fail = [](std::string message) {
    return Result<ThreadDefinition>::failure(Error::validation("thread_json", std::move(message)));
  };
  try {
    auto fields = value;
    fields.erase("start");
    if (!exact(fields, {"profile", "length", "handedness", "representation"}))
      return fail("Unknown or missing Thread definition field");
    const auto hand = value.at("handedness").get<std::string>();
    const auto mode = value.at("representation").get<std::string>();
    if ((hand != "right" && hand != "left") || (mode != "cosmetic" && mode != "modeled"))
      return fail("Unknown Thread handedness or representation");
    const auto& p = value.at("profile");
    ThreadProfile profile;
    const auto kind = p.at("kind").get<std::string>();
    if (kind == "custom") {
      auto fields = p;
      fields.erase("taper_angle");
      if (!exact(fields, {"kind", "system", "designation", "tolerance", "major_diameter",
                          "minor_diameter", "pitch", "included_angle"}))
        return fail("Malformed authored Thread profile");
      ThreadParameterProfile custom{p.at("system").get<std::string>(),
                                    p.at("designation").get<std::string>(),
                                    p.at("tolerance").get<std::string>(),
                                    ParameterId(p.at("major_diameter").get<std::string>()),
                                    ParameterId(p.at("minor_diameter").get<std::string>()),
                                    ParameterId(p.at("pitch").get<std::string>()),
                                    ParameterId(p.at("included_angle").get<std::string>()),
                                    {}};
      if (p.contains("taper_angle"))
        custom.taper_angle = ParameterId(p.at("taper_angle").get<std::string>());
      profile = std::move(custom);
    } else if (kind == "standard") {
      if (!exact(p, {"kind", "table", "row", "columns"}))
        return fail("Malformed pinned Thread profile");
      auto key = engineering_resource_key_from_json(p.at("table"));
      if (!key)
        return Result<ThreadDefinition>::failure(key.error());
      const auto& c = p.at("columns");
      auto fields = c;
      fields.erase("taper_angle");
      if (!exact(fields, {"system", "designation", "tolerance", "major_diameter", "minor_diameter",
                          "pitch", "included_angle"}))
        return fail("Malformed pinned Thread columns");
      ThreadStandardColumns columns{
          c.at("system").get<std::string>(),         c.at("designation").get<std::string>(),
          c.at("tolerance").get<std::string>(),      c.at("major_diameter").get<std::string>(),
          c.at("minor_diameter").get<std::string>(), c.at("pitch").get<std::string>(),
          c.at("included_angle").get<std::string>(), {}};
      if (c.contains("taper_angle"))
        columns.taper_angle = c.at("taper_angle").get<std::string>();
      profile = ThreadStandardProfile{std::move(key.value()), p.at("row").get<std::string>(),
                                      std::move(columns)};
    } else
      return fail("Unknown Thread profile kind");
    std::optional<ParameterId> start;
    if (value.contains("start"))
      start = ParameterId(value.at("start").get<std::string>());
    return ThreadDefinition::create(
        std::move(profile), ParameterId(value.at("length").get<std::string>()), std::move(start),
        hand == "right" ? ThreadHandedness::Right : ThreadHandedness::Left,
        mode == "cosmetic" ? ThreadRepresentation::Cosmetic : ThreadRepresentation::Modeled);
  } catch (const std::exception& error) {
    return fail(error.what());
  }
}
} // namespace blcad::detail
