#include "blcad/core/advanced_fillet_intent.hpp"
#include "blcad/core/edge_treatment_feature.hpp"
#include <type_traits>
namespace blcad {
namespace {
Result<std::size_t> identity(const FeatureId& id, const std::string& name, const BodyId& body) {
  if (id.empty() || name.empty() || body.empty())
    return Result<std::size_t>::failure(
        Error::validation(id.value(), "Fillet requires a feature ID, name and Solid target"));
  return Result<std::size_t>::success(1);
}
} // namespace
Result<FilletFeature> FilletFeature::create_face_blend(FeatureId id, std::string name, BodyId body,
                                                       FaceBlendIntent intent) {
  const auto valid = validate_face_blend_intent(intent);
  if (!valid)
    return Result<FilletFeature>::failure(valid.error());
  const auto ids = identity(id, name, body);
  if (!ids)
    return Result<FilletFeature>::failure(ids.error());
  FilletFeature feature(std::move(id), std::move(name), std::move(body), {}, intent.size);
  feature.advanced_ =
      std::make_shared<const AdvancedFilletIntent>(AdvancedFilletIntent{std::move(intent)});
  return Result<FilletFeature>::success(std::move(feature));
}
Result<FilletFeature> FilletFeature::create_full_round(FeatureId id, std::string name, BodyId body,
                                                       FullRoundIntent intent) {
  const auto valid = validate_full_round_intent(intent);
  if (!valid)
    return Result<FilletFeature>::failure(valid.error());
  const auto ids = identity(id, name, body);
  if (!ids)
    return Result<FilletFeature>::failure(ids.error());
  FilletFeature feature(std::move(id), std::move(name), std::move(body), {}, {});
  feature.advanced_ =
      std::make_shared<const AdvancedFilletIntent>(AdvancedFilletIntent{std::move(intent)});
  return Result<FilletFeature>::success(std::move(feature));
}
std::vector<FaceReference> FilletFeature::faces() const {
  if (!advanced_)
    return {};
  return std::visit(
      [](const auto& value) {
        std::vector<FaceReference> faces;
        using T = std::decay_t<decltype(value)>;
        if constexpr (!std::is_same_v<T, VariableFilletIntent>) {
          faces = value.first.faces;
          faces.insert(faces.end(), value.second.faces.begin(), value.second.faces.end());
          if constexpr (std::is_same_v<T, FullRoundIntent>)
            faces.insert(faces.end(), value.middle.faces.begin(), value.middle.faces.end());
        }
        return faces;
      },
      advanced_->definition);
}
std::vector<PathCurveId> FilletFeature::hold_lines() const {
  if (!advanced_)
    return {};
  return std::visit(
      [](const auto& value) {
        std::vector<PathCurveId> paths;
        using T = std::decay_t<decltype(value)>;
        if constexpr (!std::is_same_v<T, VariableFilletIntent>)
          for (const auto& line : value.hold_lines)
            paths.push_back(line.path);
        return paths;
      },
      advanced_->definition);
}
} // namespace blcad
