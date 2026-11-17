#include "face_fillet_json.hpp"
#include <stdexcept>
namespace blcad::detail {
namespace {
using json = nlohmann::json;
Error error(const std::string& message) {
  return Error::validation("fillet.definition", message);
}
json group_json(const FilletFaceSet& group, const FilletFaceWriter& write) {
  json faces = json::array();
  for (const auto& face : group.faces)
    faces.push_back(write(face));
  return {{"id", group.id}, {"name", group.name}, {"faces", faces}};
}
Result<FilletFaceSet> group(const json& value, PartFeatureInputRole role,
                            const FilletFaceReader& read) {
  if (!value.is_object() || value.size() != 3 || !value.at("id").is_string() ||
      !value.at("name").is_string() || !value.at("faces").is_array())
    return Result<FilletFaceSet>::failure(error("Malformed Fillet face set"));
  FilletFaceSet result{value.at("id").get<std::string>(), value.at("name").get<std::string>(), {}};
  for (const auto& item : value.at("faces")) {
    auto face = read(item, role);
    if (!face)
      return Result<FilletFaceSet>::failure(face.error());
    result.faces.push_back(std::move(face.value()));
  }
  return Result<FilletFaceSet>::success(std::move(result));
}
json lines_json(const std::vector<FilletHoldLine>& lines) {
  json result = json::array();
  for (const auto& line : lines)
    result.push_back({{"id", line.id},
                      {"side", line.side == FilletSide::First ? "first" : "second"},
                      {"path", line.path.value()}});
  return result;
}
Result<std::vector<FilletHoldLine>> lines(const json& value) {
  if (!value.is_array())
    return Result<std::vector<FilletHoldLine>>::failure(
        error("Hold lines require an ordered array"));
  std::vector<FilletHoldLine> result;
  for (const auto& item : value) {
    if (!item.is_object() || item.size() != 3 || !item.at("id").is_string() ||
        !item.at("side").is_string() || !item.at("path").is_string() ||
        (item.at("side") != "first" && item.at("side") != "second"))
      return Result<std::vector<FilletHoldLine>>::failure(error("Malformed Fillet hold line"));
    result.push_back({item.at("id").get<std::string>(),
                      item.at("side") == "first" ? FilletSide::First : FilletSide::Second,
                      PathCurveId(item.at("path").get<std::string>())});
  }
  return Result<std::vector<FilletHoldLine>>::success(std::move(result));
}
Result<FilletContinuity> continuity(const json& value) {
  if (value == "g1")
    return Result<FilletContinuity>::success(FilletContinuity::Tangent);
  if (value == "g2")
    return Result<FilletContinuity>::success(FilletContinuity::Curvature);
  return Result<FilletContinuity>::failure(error("Unsupported Fillet continuity"));
}
} // namespace
nlohmann::json face_blend_to_json(const FaceBlendIntent& value, const FilletFaceWriter& write) {
  return {{"first", group_json(value.first, write)},
          {"second", group_json(value.second, write)},
          {"continuity", value.continuity == FilletContinuity::Tangent ? "g1" : "g2"},
          {"size",
           {{"kind", value.size_kind == FaceBlendSizeKind::Radius ? "radius" : "contact_setback"},
            {"parameter", value.size.value()}}},
          {"hold_lines", lines_json(value.hold_lines)}};
}
nlohmann::json full_round_to_json(const FullRoundIntent& value, const FilletFaceWriter& write) {
  return {{"first", group_json(value.first, write)},
          {"second", group_json(value.second, write)},
          {"middle", group_json(value.middle, write)},
          {"continuity", value.continuity == FilletContinuity::Tangent ? "g1" : "g2"},
          {"hold_lines", lines_json(value.hold_lines)}};
}
Result<FaceBlendIntent> face_blend_from_json(const nlohmann::json& value,
                                             const FilletFaceReader& read) {
  try {
    if (!value.is_object() || value.size() != 5)
      return Result<FaceBlendIntent>::failure(error("Malformed face-blend definition"));
    auto first = group(value.at("first"), PartFeatureInputRole::FilletSupportFace, read);
    if (!first)
      return Result<FaceBlendIntent>::failure(first.error());
    auto second = group(value.at("second"), PartFeatureInputRole::FilletSupportFace, read);
    if (!second)
      return Result<FaceBlendIntent>::failure(second.error());
    auto c = continuity(value.at("continuity"));
    if (!c)
      return Result<FaceBlendIntent>::failure(c.error());
    auto hold = lines(value.at("hold_lines"));
    if (!hold)
      return Result<FaceBlendIntent>::failure(hold.error());
    const auto& size = value.at("size");
    if (!size.is_object() || size.size() != 2 || !size.at("parameter").is_string() ||
        (size.at("kind") != "radius" && size.at("kind") != "contact_setback"))
      return Result<FaceBlendIntent>::failure(error("Malformed face-blend size"));
    FaceBlendIntent result{std::move(first.value()),
                           std::move(second.value()),
                           c.value(),
                           size.at("kind") == "radius" ? FaceBlendSizeKind::Radius
                                                       : FaceBlendSizeKind::ContactSetback,
                           ParameterId(size.at("parameter").get<std::string>()),
                           std::move(hold.value())};
    auto valid = validate_face_blend_intent(result);
    if (!valid)
      return Result<FaceBlendIntent>::failure(valid.error());
    return Result<FaceBlendIntent>::success(std::move(result));
  } catch (const nlohmann::json::exception& e) {
    return Result<FaceBlendIntent>::failure(error(e.what()));
  }
}
Result<FullRoundIntent> full_round_from_json(const nlohmann::json& value,
                                             const FilletFaceReader& read) {
  try {
    if (!value.is_object() || value.size() != 5)
      return Result<FullRoundIntent>::failure(error("Malformed full-round definition"));
    auto first = group(value.at("first"), PartFeatureInputRole::FilletSupportFace, read);
    if (!first)
      return Result<FullRoundIntent>::failure(first.error());
    auto second = group(value.at("second"), PartFeatureInputRole::FilletSupportFace, read);
    if (!second)
      return Result<FullRoundIntent>::failure(second.error());
    auto middle = group(value.at("middle"), PartFeatureInputRole::FilletMiddleFace, read);
    if (!middle)
      return Result<FullRoundIntent>::failure(middle.error());
    auto c = continuity(value.at("continuity"));
    if (!c)
      return Result<FullRoundIntent>::failure(c.error());
    auto hold = lines(value.at("hold_lines"));
    if (!hold)
      return Result<FullRoundIntent>::failure(hold.error());
    FullRoundIntent result{std::move(first.value()), std::move(second.value()),
                           std::move(middle.value()), c.value(), std::move(hold.value())};
    auto valid = validate_full_round_intent(result);
    if (!valid)
      return Result<FullRoundIntent>::failure(valid.error());
    return Result<FullRoundIntent>::success(std::move(result));
  } catch (const nlohmann::json::exception& e) {
    return Result<FullRoundIntent>::failure(error(e.what()));
  }
}
nlohmann::json setback_to_json(const FilletSetbackCorner& value) {
  return {{"kind", "setback"},
          {"id", value.id},
          {"groups", value.groups},
          {"distance_parameter", value.distance.value()}};
}
Result<FilletSetbackCorner> setback_from_json(const nlohmann::json& value) {
  try {
    if (!value.is_object() || value.size() != 4 || value.at("kind") != "setback" ||
        !value.at("id").is_string() || !value.at("distance_parameter").is_string() ||
        !value.at("groups").is_array() || value.at("groups").size() != 3)
      return Result<FilletSetbackCorner>::failure(error("Malformed setback corner"));
    for (const auto& group : value.at("groups"))
      if (!group.is_string())
        return Result<FilletSetbackCorner>::failure(error("Setback group IDs must be strings"));
    return Result<FilletSetbackCorner>::success(
        {value.at("id").get<std::string>(), value.at("groups").get<std::array<std::string, 3>>(),
         ParameterId(value.at("distance_parameter").get<std::string>())});
  } catch (const nlohmann::json::exception& e) {
    return Result<FilletSetbackCorner>::failure(error(e.what()));
  }
}
} // namespace blcad::detail
