#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/part_document.hpp"
#include "fillet_parameter_validation.hpp"
#include <cmath>
#include <memory>
#include <set>
#include <type_traits>
namespace blcad {
Result<FilletFeature> FilletFeature::create_variable(FeatureId id, std::string name, BodyId target,
                                                     VariableFilletIntent intent) {
  const auto valid = validate_variable_fillet_intent(intent);
  if (!valid)
    return Result<FilletFeature>::failure(valid.error());
  std::vector<EdgeReference> edges;
  for (const auto& group : intent.groups)
    for (const auto& member : group.edges)
      edges.push_back(member.edge);
  auto result = create(std::move(id), std::move(name), std::move(target), std::move(edges),
                       intent.groups.front().stations.front().radius);
  if (result)
    result.value().advanced_ =
        std::make_shared<const AdvancedFilletIntent>(AdvancedFilletIntent{std::move(intent)});
  return result;
}
const AdvancedFilletIntent* FilletFeature::advanced_intent() const noexcept {
  return advanced_.get();
}
std::vector<ParameterId> FilletFeature::parameter_dependencies() const {
  if (!advanced_)
    return {radius_parameter_};
  std::vector<ParameterId> result;
  std::set<std::string> seen;
  const auto add = [&](const ParameterId& id) {
    if (seen.insert(id.value()).second)
      result.push_back(id);
  };
  std::visit(
      [&](const auto& intent) {
        using T = std::decay_t<decltype(intent)>;
        if constexpr (std::is_same_v<T, VariableFilletIntent>) {
          for (const auto& group : intent.groups)
            for (const auto& station : group.stations)
              add(station.radius);
          if (intent.setback)
            add(intent.setback->distance);
        } else if constexpr (std::is_same_v<T, FaceBlendIntent>)
          add(intent.size);
      },
      advanced_->definition);
  return result;
}
namespace detail {
Result<std::size_t> validate_fillet_parameters(const PartDocument& part,
                                               const FilletFeature& feature) {
  for (const auto& id : feature.parameter_dependencies()) {
    const auto* radius = part.find_parameter(id);
    if (!radius || radius->type() != ParameterType::Length)
      return Result<std::size_t>::failure(
          Error::validation(feature.advanced_intent() ? id.value() : feature.id().value(),
                            feature.advanced_intent()
                                ? "Advanced Fillet size must reference an existing length parameter"
                                : "fillet radius must reference an existing length parameter"));
    if (feature.advanced_intent() &&
        (!std::isfinite(radius->value().millimeters()) || radius->value().millimeters() <= 0))
      return Result<std::size_t>::failure(
          Error::validation(id.value(), "Advanced Fillet sizes require positive Length values"));
  }
  return Result<std::size_t>::success(feature.parameter_dependencies().size());
}
} // namespace detail
} // namespace blcad
