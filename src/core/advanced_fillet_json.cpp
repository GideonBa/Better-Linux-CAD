#include "advanced_fillet_json.hpp"
#include <stdexcept>
namespace blcad::detail {
using json = nlohmann::json;
json variable_fillet_to_json(const VariableFilletIntent& intent, const FilletEdgeWriter& write) {
  json groups = json::array();
  for (const auto& group : intent.groups) {
    json value{{"id", group.id},
               {"name", group.name},
               {"propagation",
                group.propagation == FilletPropagation::Explicit ? "explicit" : "tangent_chain"},
               {"interpolation", group.interpolation == FilletRadiusInterpolation::Linear
                                     ? "linear"
                                     : "monotone_cubic"},
               {"edges", json::array()},
               {"stations", json::array()},
               {"station_basis_reversed", group.station_basis_reversed}};
    for (const auto& edge : group.edges)
      value["edges"].push_back({{"edge", write(edge.edge)}, {"reversed", edge.reversed}});
    for (const auto& station : group.stations)
      value["stations"].push_back(
          {{"id", station.id},
           {"location", station.location == FilletStationLocation::Start ? "start"
                        : station.location == FilletStationLocation::End ? "end"
                                                                         : "fraction"},
           {"fraction", station.fraction},
           {"radius", station.radius.value()}});
    groups.push_back(std::move(value));
  }
  return groups;
}
Result<VariableFilletIntent> variable_fillet_from_json(const json& groups,
                                                       const FilletEdgeReader& read) {
  const auto fail = [](std::string message) {
    return Result<VariableFilletIntent>::failure(
        Error::validation("fillet.groups", std::move(message)));
  };
  try {
    if (!groups.is_array())
      return fail("Variable Fillet groups must be an ordered array");
    VariableFilletIntent result;
    for (const auto& value : groups) {
      if (!value.is_object() || value.size() != 7 || !value.at("id").is_string() ||
          !value.at("name").is_string() || !value.at("propagation").is_string() ||
          !value.at("interpolation").is_string() || !value.at("edges").is_array() ||
          !value.at("stations").is_array() || !value.at("station_basis_reversed").is_boolean())
        return fail("Malformed variable Fillet group");
      const auto propagation = value.at("propagation").get<std::string>(),
                 interpolation = value.at("interpolation").get<std::string>();
      if (propagation != "explicit" && propagation != "tangent_chain")
        return fail("Unsupported Fillet propagation");
      if (interpolation != "linear" && interpolation != "monotone_cubic")
        return fail("Unsupported radius interpolation");
      FilletEdgeGroup group{value.at("id").get<std::string>(),
                            value.at("name").get<std::string>(),
                            {},
                            propagation == "explicit" ? FilletPropagation::Explicit
                                                      : FilletPropagation::TangentChain,
                            interpolation == "linear" ? FilletRadiusInterpolation::Linear
                                                      : FilletRadiusInterpolation::MonotoneCubic,
                            {}};
      group.station_basis_reversed = value.at("station_basis_reversed").get<bool>();
      for (const auto& member : value.at("edges")) {
        if (!member.is_object() || member.size() != 2 || !member.at("reversed").is_boolean())
          return fail("Malformed oriented Fillet edge");
        auto edge = read(member.at("edge"));
        if (!edge)
          return Result<VariableFilletIntent>::failure(edge.error());
        group.edges.push_back({std::move(edge.value()), member.at("reversed").get<bool>()});
      }
      for (const auto& entry : value.at("stations")) {
        if (!entry.is_object() || entry.size() != 4 || !entry.at("id").is_string() ||
            !entry.at("location").is_string() || !entry.at("fraction").is_number() ||
            !entry.at("radius").is_string())
          return fail("Malformed Fillet radius station");
        const auto location = entry.at("location").get<std::string>();
        if (location != "start" && location != "end" && location != "fraction")
          return fail("Unsupported Fillet radius station location");
        group.stations.push_back({entry.at("id").get<std::string>(),
                                  location == "start" ? FilletStationLocation::Start
                                  : location == "end" ? FilletStationLocation::End
                                                      : FilletStationLocation::Fraction,
                                  entry.at("fraction").get<double>(),
                                  ParameterId(entry.at("radius").get<std::string>())});
      }
      result.groups.push_back(std::move(group));
    }
    const auto valid = validate_variable_fillet_intent(result);
    if (!valid)
      return Result<VariableFilletIntent>::failure(valid.error());
    return Result<VariableFilletIntent>::success(std::move(result));
  } catch (const json::exception& error) {
    return fail(std::string("Malformed variable Fillet intent: ") + error.what());
  }
}
} // namespace blcad::detail
