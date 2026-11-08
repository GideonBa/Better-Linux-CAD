#include "blcad/core/hole_feature.hpp"

#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <type_traits>
#include <utility>

namespace blcad {
namespace {
bool nonblank(const std::string& value) {
  return value.find_first_not_of(" \t\r\n") != std::string::npos;
}
bool valid_diameter(const HoleDiameter& diameter) {
  if (const auto* parameter = std::get_if<ParameterId>(&diameter))
    return !parameter->empty();
  const auto& standard = std::get<HoleStandardSelection>(diameter);
  return nonblank(standard.row) && nonblank(standard.diameter_column);
}
bool valid_extent(const HoleExtent& extent) {
  return std::visit(
      [](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, HoleBlindExtent>)
          return !value.depth.empty();
        else if constexpr (std::is_same_v<T, HoleSymmetricExtent>)
          return !value.total_depth.empty();
        else if constexpr (std::is_same_v<T, HoleUpToBodyExtent>)
          return !value.body.empty();
        else if constexpr (std::is_same_v<T, HoleToFaceExtent>)
          return value.face.role() == PartFeatureInputRole::HoleLimitFace;
        else
          return true;
      },
      extent);
}
bool positive_length(const PartDocument& part, const ParameterId& id) {
  const auto* p = part.find_parameter(id);
  return p && p->type() == ParameterType::Length && p->value().is_positive_length();
}
} // namespace

std::string_view to_string(HoleKind kind) noexcept {
  switch (kind) {
  case HoleKind::Plain:
    return "plain";
  case HoleKind::Clearance:
    return "clearance";
  }
  return "unknown";
}
std::string_view to_string(HoleDirection direction) noexcept {
  switch (direction) {
  case HoleDirection::SupportNormal:
    return "support_normal";
  case HoleDirection::OppositeSupportNormal:
    return "opposite_support_normal";
  }
  return "unknown";
}

HoleFeature::HoleFeature(FeatureId id, std::string name, HoleKind kind, BodyId target_body,
                         PlaneReference support, std::vector<HolePlacement> placements,
                         HoleDiameter diameter, HoleExtent extent, HoleDirection direction,
                         HoleTip tip)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), target_body_(std::move(target_body)),
      support_(std::move(support)), placements_(std::move(placements)),
      diameter_(std::move(diameter)), extent_(std::move(extent)), direction_(direction),
      tip_(std::move(tip)) {}

Result<HoleFeature> HoleFeature::create(FeatureId id, std::string name, HoleKind kind,
                                        BodyId target_body, PlaneReference support,
                                        std::vector<HolePlacement> placements,
                                        HoleDiameter diameter, HoleExtent extent,
                                        HoleDirection direction, HoleTip tip) {
  const auto fail = [&](std::string message) {
    return Result<HoleFeature>::failure(
        Error::validation(id.empty() ? "hole" : id.value(), std::move(message)));
  };
  if (id.empty() || !nonblank(name) || target_body.empty())
    return fail("Hole requires a feature identity, name and target Body");
  if ((kind != HoleKind::Plain && kind != HoleKind::Clearance) ||
      (direction != HoleDirection::SupportNormal &&
       direction != HoleDirection::OppositeSupportNormal))
    return fail("Unknown Hole kind or direction");
  if (support.role() != PartFeatureInputRole::HoleSupportPlane)
    return fail("Hole support requires the hole_support_plane role");
  if (placements.empty())
    return fail("Hole requires at least one placement");
  std::set<std::string> ids;
  for (const auto& placement : placements) {
    if (placement.id.empty() || !ids.insert(placement.id.value()).second)
      return fail("Hole placement identities must be nonempty and unique within the feature");
    if (const auto* point = std::get_if<Point2>(&placement.source)) {
      if (!std::isfinite(point->x) || !std::isfinite(point->y))
        return fail("Owned Hole placement coordinates must be finite");
    } else {
      const auto& reference = std::get<HoleSketchPointReference>(placement.source);
      if (reference.sketch.empty() || reference.point.empty())
        return fail("Hole Sketch placement requires Sketch and point identities");
    }
  }
  if (!valid_diameter(diameter))
    return fail("Hole diameter selection is incomplete");
  if (!valid_extent(extent))
    return fail("Hole extent input is incomplete or has the wrong role");
  if (const auto* cone = std::get_if<HoleConicalTip>(&tip))
    if (cone->included_angle.empty() || !std::holds_alternative<HoleBlindExtent>(extent))
      return fail("A conical drill tip requires a blind extent and included-angle parameter");
  return Result<HoleFeature>::success(HoleFeature(
      std::move(id), std::move(name), kind, std::move(target_body), std::move(support),
      std::move(placements), std::move(diameter), std::move(extent), direction, std::move(tip)));
}

std::vector<ParameterId> HoleFeature::parameter_dependencies() const {
  std::vector<ParameterId> result;
  const auto append = [&](const ParameterId& id) {
    if (std::find(result.begin(), result.end(), id) == result.end())
      result.push_back(id);
  };
  if (const auto* diameter = std::get_if<ParameterId>(&diameter_))
    append(*diameter);
  if (const auto* blind = std::get_if<HoleBlindExtent>(&extent_))
    append(blind->depth);
  if (const auto* symmetric = std::get_if<HoleSymmetricExtent>(&extent_))
    append(symmetric->total_depth);
  if (const auto* cone = std::get_if<HoleConicalTip>(&tip_))
    append(cone->included_angle);
  return result;
}

Result<ResolvedHoleDiameter> resolve_hole_diameter(const PartDocument& part,
                                                   const HoleDiameter& diameter) {
  const auto fail = [&](std::string message) {
    return Result<ResolvedHoleDiameter>::failure(
        Error::validation(part.id().value(), std::move(message)));
  };
  if (!valid_diameter(diameter))
    return fail("Hole diameter selection is incomplete");
  if (const auto* id = std::get_if<ParameterId>(&diameter)) {
    if (!positive_length(part, *id))
      return fail("Hole diameter requires a positive Length parameter");
    return Result<ResolvedHoleDiameter>::success({part.find_parameter(*id)->value(), std::nullopt});
  }
  const auto& selection = std::get<HoleStandardSelection>(diameter);
  const auto* table = part.engineering_resources().find_table(selection.table);
  if (!table)
    return fail("Pinned Hole standard table revision is unavailable");
  const auto column =
      std::find_if(table->columns().begin(), table->columns().end(),
                   [&](const auto& c) { return c.key == selection.diameter_column; });
  if (column == table->columns().end() || column->kind != EngineeringTableValueKind::Length)
    return fail("Hole diameter must select a Length standard-table column");
  const auto* row = table->find_row(selection.row);
  if (!row)
    return fail("Pinned Hole standard row is unavailable");
  const auto cell = std::find_if(row->cells.begin(), row->cells.end(), [&](const auto& c) {
    return c.column == selection.diameter_column;
  });
  if (cell == row->cells.end())
    return fail("Pinned Hole diameter cell is missing");
  const auto* quantity = std::get_if<Quantity>(&cell->value);
  if (!quantity || !quantity->is_positive_length())
    return fail("Pinned Hole diameter cell must be a positive Length");
  return Result<ResolvedHoleDiameter>::success({*quantity, cell->provenance});
}

Result<std::size_t> validate_hole_feature_inputs(const PartDocument& part,
                                                 const HoleFeature& feature) {
  const auto fail = [&](std::string message) {
    return Result<std::size_t>::failure(
        Error::validation(feature.id().value(), std::move(message)));
  };
  if (part.is_reference_part())
    return fail("Reference Part Hole intent is read-only");
  const auto* body = part.find_body(feature.target_body());
  if (!body || body->kind() != BodyKind::Solid)
    return fail("Hole target must be an existing solid Body");
  auto support = validate_part_feature_input_reference(part, feature.support());
  if (!support)
    return support;
  auto diameter = resolve_hole_diameter(part, feature.diameter());
  if (!diameter)
    return Result<std::size_t>::failure(diameter.error());
  for (const auto& placement : feature.placements())
    if (const auto* ref = std::get_if<HoleSketchPointReference>(&placement.source)) {
      const auto* sketch = part.find_sketch(ref->sketch);
      if (!sketch || (!sketch->find_point(ref->point) && !sketch->find_projected_point(ref->point)))
        return fail("Hole placement must reference an existing Sketch point");
    }
  if (const auto* blind = std::get_if<HoleBlindExtent>(&feature.extent()))
    if (!positive_length(part, blind->depth))
      return fail("Blind Hole depth requires a positive Length");
  if (const auto* symmetric = std::get_if<HoleSymmetricExtent>(&feature.extent()))
    if (!positive_length(part, symmetric->total_depth))
      return fail("Symmetric Hole total depth requires a positive Length");
  if (const auto* face = std::get_if<HoleToFaceExtent>(&feature.extent())) {
    auto valid = validate_part_feature_input_reference(part, face->face);
    if (!valid)
      return valid;
  }
  if (const auto* limit = std::get_if<HoleUpToBodyExtent>(&feature.extent())) {
    const auto* limit_body = part.find_body(limit->body);
    if (!limit_body || limit_body->kind() != BodyKind::Solid ||
        limit->body == feature.target_body())
      return fail("Up-to-Body Hole limit must be a distinct existing solid Body");
  }
  if (const auto* cone = std::get_if<HoleConicalTip>(&feature.tip())) {
    const auto* angle = part.find_parameter(cone->included_angle);
    if (!angle || angle->type() != ParameterType::Angle || angle->value().degrees() <= 0 ||
        angle->value().degrees() >= 180)
      return fail(
          "Conical drill included angle must be an Angle strictly between 0 and 180 degrees");
  }
  return Result<std::size_t>::success(1);
}
} // namespace blcad
