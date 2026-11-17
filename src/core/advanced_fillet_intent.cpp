#include "blcad/core/advanced_fillet_intent.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <type_traits>
namespace blcad {
namespace {
std::string identity(const EdgeReference& edge) {
  return std::visit(
      [](const auto& source) -> std::string {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, ImportedTopologyReference>)
          return make_imported_topology_target_spelling(source).value();
        else if constexpr (std::is_same_v<T, ResultEdgeReference>)
          return source.identity();
        else if constexpr (std::is_same_v<T, SemanticEdgeReference>)
          return source.node_id();
        else
          return source.source_feature().value() + ":" + source.source_profile().value() + ":" +
                 std::string(to_string(source.edge()));
      },
      edge.source());
}
} // namespace
double FilletRadiusStation::normalized_length() const noexcept {
  switch (location) {
  case FilletStationLocation::Start:
    return 0;
  case FilletStationLocation::End:
    return 1;
  case FilletStationLocation::Fraction:
    return fraction;
  }
  return fraction;
}
Result<std::size_t> validate_variable_fillet_intent(const VariableFilletIntent& intent) {
  const auto fail = [](std::string id, std::string message) {
    return Result<std::size_t>::failure(Error::validation(std::move(id), std::move(message)));
  };
  if (intent.groups.empty())
    return fail("fillet.groups", "Variable Fillet requires at least one ordered edge group");
  std::set<std::string> groups, stations, edges;
  for (const auto& group : intent.groups) {
    if (group.id.empty() || group.name.empty() || !groups.insert(group.id).second)
      return fail(group.id,
                  "Fillet group IDs must be nonempty and unique; every group needs a name");
    if (group.edges.empty())
      return fail(group.id, "Fillet group requires at least one semantic edge");
    if (group.propagation != FilletPropagation::Explicit &&
        group.propagation != FilletPropagation::TangentChain)
      return fail(group.id, "Unsupported Fillet propagation rule");
    if (group.interpolation != FilletRadiusInterpolation::Linear &&
        group.interpolation != FilletRadiusInterpolation::MonotoneCubic)
      return fail(group.id, "Unsupported Fillet radius interpolation");
    for (const auto& member : group.edges) {
      if (member.edge.role() != PartFeatureInputRole::FilletEdge)
        return fail(group.id, "Fillet group requires Fillet edge roles");
      if (!edges.insert(identity(member.edge)).second)
        return fail(group.id, "A semantic edge can belong to only one Fillet group");
    }
    if (group.stations.size() < 2)
      return fail(group.id, "Variable Fillet requires start and end radius stations");
    double previous = -1;
    for (const auto& station : group.stations) {
      if (station.id.empty() || !stations.insert(station.id).second || station.radius.empty())
        return fail(
            group.id,
            "Radius station IDs must be globally unique and radius parameters must be explicit");
      if (station.location != FilletStationLocation::Fraction &&
          station.location != FilletStationLocation::Start &&
          station.location != FilletStationLocation::End)
        return fail(station.id, "Unsupported radius station location");
      if (!std::isfinite(station.fraction) || station.fraction < 0 || station.fraction > 1)
        return fail(station.id, "Radius station fraction must lie in [0, 1]");
      if ((station.location == FilletStationLocation::Start && station.fraction != 0) ||
          (station.location == FilletStationLocation::End && station.fraction != 1))
        return fail(station.id,
                    "Semantic endpoint radius stations require canonical endpoint fractions");
      const double value = station.normalized_length();
      if (value <= previous)
        return fail(station.id, "Radius stations must be strictly ordered in normalized true curve "
                                "length; duplicates are ambiguous");
      previous = value;
    }
    if (group.stations.front().normalized_length() != 0 ||
        group.stations.back().normalized_length() != 1)
      return fail(group.id, "Radius law must cover the complete oriented contour from zero to one");
  }
  if (intent.setback) {
    const auto& corner = *intent.setback;
    if (corner.id.empty() || corner.distance.empty() || intent.groups.size() != 3)
      return fail(
          corner.id,
          "Setback corners require a stable ID, distance and exactly three explicit edge groups");
    std::set<std::string> selected;
    std::optional<ParameterId> radius;
    for (const auto& id : corner.groups) {
      const auto found = std::find_if(intent.groups.begin(), intent.groups.end(),
                                      [&](const auto& group) { return group.id == id; });
      if (!selected.insert(id).second || found == intent.groups.end() || found->edges.size() != 1 ||
          found->propagation != FilletPropagation::Explicit)
        return fail(corner.id, "Setback requires three distinct explicit single-edge groups");
      for (const auto& station : found->stations) {
        if (!radius)
          radius = station.radius;
        if (station.radius != *radius)
          return fail(corner.id, "Supported setback groups must share one constant radius binding");
      }
    }
  }
  return Result<std::size_t>::success(intent.groups.size());
}
FilletEdgeGroup reverse_fillet_group(FilletEdgeGroup group) {
  std::reverse(group.edges.begin(), group.edges.end());
  for (auto& member : group.edges)
    member.reversed = !member.reversed;
  group.station_basis_reversed = !group.station_basis_reversed;
  return group;
}
double fillet_chain_fraction(const FilletEdgeGroup& group,
                             const FilletRadiusStation& station) noexcept {
  const double value = station.normalized_length();
  return group.station_basis_reversed ? 1 - value : value;
}
} // namespace blcad
