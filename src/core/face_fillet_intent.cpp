#include "blcad/core/advanced_fillet_intent.hpp"
#include <algorithm>
#include <set>
#include <type_traits>
namespace blcad {
namespace {
std::string identity(const FaceReference& face) {
  return std::visit(
      [](const auto& source) -> std::string {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, ImportedTopologyReference>)
          return make_imported_topology_target_spelling(source).value();
        else if constexpr (std::is_same_v<T, ResultFaceReference>)
          return source.identity();
        else if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return source.source_feature().value() + ":" + std::string(to_string(source.face()));
        else
          return source.source_feature().value() + ":" + source.source_profile().value() + ":" +
                 std::string(to_string(source.face()));
      },
      face.source());
}
Result<std::size_t> invalid(const std::string& id, const std::string& message) {
  return Result<std::size_t>::failure(Error::validation(id, message));
}
Result<std::size_t>
validate(const std::vector<std::pair<const FilletFaceSet*, PartFeatureInputRole>>& sets,
         FilletContinuity continuity, const std::vector<FilletHoldLine>& lines) {
  if (continuity != FilletContinuity::Tangent && continuity != FilletContinuity::Curvature)
    return invalid("fillet.continuity", "Unsupported Fillet continuity");
  std::set<std::string> ids, faces;
  std::size_t count = 0;
  for (const auto& [group, role] : sets) {
    if (group->id.empty() || group->name.empty() || !ids.insert(group->id).second ||
        group->faces.empty())
      return invalid(group->id,
                     "Fillet face sets require unique IDs, names and nonempty ordered faces");
    for (const auto& face : group->faces) {
      if (face.role() != role)
        return invalid(group->id, "Fillet face reference has an incompatible side or middle role");
      if (!faces.insert(identity(face)).second)
        return invalid(group->id, "A face may occur in only one Fillet role");
      ++count;
    }
  }
  if (lines.size() > 2)
    return invalid("fillet.hold_lines", "At most one hold line per side is supported");
  std::set<int> sides;
  std::set<std::string> paths;
  ids.clear();
  for (const auto& line : lines) {
    if (line.id.empty() || line.path.empty() || !ids.insert(line.id).second ||
        !paths.insert(line.path.value()).second ||
        (line.side != FilletSide::First && line.side != FilletSide::Second) ||
        !sides.insert(int(line.side)).second)
      return invalid(line.id,
                     "Hold lines require unique IDs, owned curves and distinct side roles");
  }
  return Result<std::size_t>::success(count);
}
} // namespace
Result<std::size_t> validate_face_blend_intent(const FaceBlendIntent& value) {
  auto valid = validate({{&value.first, PartFeatureInputRole::FilletSupportFace},
                         {&value.second, PartFeatureInputRole::FilletSupportFace}},
                        value.continuity, value.hold_lines);
  if (!valid)
    return valid;
  if (value.size.empty() ||
      (value.continuity == FilletContinuity::Tangent &&
       value.size_kind != FaceBlendSizeKind::Radius) ||
      (value.continuity == FilletContinuity::Curvature &&
       value.size_kind != FaceBlendSizeKind::ContactSetback))
    return invalid(
        "fillet.size",
        "G1 face blends require a radius; G2 face blends require an explicit contact-setback size");
  return valid;
}
Result<std::size_t> validate_full_round_intent(const FullRoundIntent& value) {
  return validate({{&value.first, PartFeatureInputRole::FilletSupportFace},
                   {&value.second, PartFeatureInputRole::FilletSupportFace},
                   {&value.middle, PartFeatureInputRole::FilletMiddleFace}},
                  value.continuity, value.hold_lines);
}
} // namespace blcad
