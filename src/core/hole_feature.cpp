#include "blcad/core/hole_feature.hpp"

#include "blcad/core/part_document.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
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
template <typename Manufacturing, typename Visitor>
void visit_manufacturing_dimensions(Manufacturing& manufacturing, Visitor visitor) {
  std::visit(
      [&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, HoleCounterbore> || std::is_same_v<T, HoleSpotface>) {
          visitor(value.diameter, false);
          visitor(value.depth, false);
        } else if constexpr (std::is_same_v<T, HoleCountersink> || std::is_same_v<T, HoleCenter>) {
          visitor(value.diameter, false);
          visitor(value.included_angle, true);
        } else if constexpr (std::is_same_v<T, HoleStepped>) {
          for (auto& step : value.steps) {
            visitor(step.diameter, false);
            visitor(step.depth, false);
          }
        } else if constexpr (std::is_same_v<T, HoleFitted>) {
          visitor(value.minimum_diameter, false);
          visitor(value.maximum_diameter, false);
        }
      },
      manufacturing);
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
                         HoleTip tip, HoleManufacturing manufacturing)
    : id_(std::move(id)), name_(std::move(name)), kind_(kind), target_body_(std::move(target_body)),
      support_(std::move(support)), placements_(std::move(placements)),
      diameter_(std::move(diameter)), extent_(std::move(extent)), direction_(direction),
      tip_(std::move(tip)), manufacturing_(std::move(manufacturing)) {}

Result<HoleFeature> HoleFeature::create(FeatureId id, std::string name, HoleKind kind,
                                        BodyId target_body, PlaneReference support,
                                        std::vector<HolePlacement> placements,
                                        HoleDiameter diameter, HoleExtent extent,
                                        HoleDirection direction, HoleTip tip,
                                        HoleManufacturing manufacturing) {
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
  bool dimensions_valid = true;
  visit_manufacturing_dimensions(manufacturing, [&](const auto& dim, bool) {
    dimensions_valid = dimensions_valid && valid_diameter(dim);
  });
  if (!dimensions_valid)
    return fail("Manufacturing Hole dimension selection is incomplete");
  const bool simple = std::holds_alternative<std::monostate>(manufacturing);
  const bool fitted = std::holds_alternative<HoleFitted>(manufacturing);
  if (!simple && !fitted && std::holds_alternative<HoleSymmetricExtent>(extent))
    return fail("Entry seats and slots require a one-direction Hole extent");
  if (const auto* steps = std::get_if<HoleStepped>(&manufacturing)) {
    if (steps->steps.empty())
      return fail("Stepped Hole requires at least one authored step");
    std::set<std::string> step_ids;
    for (const auto& step : steps->steps)
      if (!nonblank(step.id) || !step_ids.insert(step.id).second)
        return fail("Hole step identities must be nonblank and unique");
  }
  if (const auto* fit = std::get_if<HoleFitted>(&manufacturing))
    if (!nonblank(fit->designation) ||
        (fit->process != HoleFitProcess::Fitted && fit->process != HoleFitProcess::Reamed))
      return fail("Fitted Hole requires a designation and supported finishing process");
  if (std::holds_alternative<HoleCenter>(manufacturing) &&
      !std::holds_alternative<HoleBlindExtent>(extent))
    return fail("Center Hole requires an explicit blind pilot depth");
  if (const auto* slot = std::get_if<HoleSlot>(&manufacturing)) {
    const auto* point =
        placements.size() == 1 ? std::get_if<Point2>(&placements.front().source) : nullptr;
    if (!point || point->x != 0 || point->y != 0 ||
        slot->profile.role() != PartFeatureInputRole::ProfileRegion ||
        (!std::holds_alternative<HoleThroughAllExtent>(extent) &&
         !std::holds_alternative<HoleBlindExtent>(extent)) ||
        !std::holds_alternative<HoleFlatTip>(tip))
      return fail("Slot Hole consumes one existing profile at its authored position with flat "
                  "Blind or ThroughAll extent");
  }
  return Result<HoleFeature>::success(
      HoleFeature(std::move(id), std::move(name), kind, std::move(target_body), std::move(support),
                  std::move(placements), std::move(diameter), std::move(extent), direction,
                  std::move(tip), std::move(manufacturing)));
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
  visit_manufacturing_dimensions(manufacturing_, [&](const auto& dim, bool) {
    if (const auto* id = std::get_if<ParameterId>(&dim))
      append(*id);
  });
  return result;
}

std::vector<EngineeringResourceKey> HoleFeature::standard_dependencies() const {
  std::vector<EngineeringResourceKey> keys;
  const auto append = [&](const auto& dimension, bool) {
    const auto* selected = std::get_if<HoleStandardSelection>(&dimension);
    if (selected && std::find(keys.begin(), keys.end(), selected->table) == keys.end())
      keys.push_back(selected->table);
  };
  append(diameter_, false);
  visit_manufacturing_dimensions(manufacturing_, append);
  return keys;
}
Result<HoleFeature> HoleFeature::remap_standard_revision(const EngineeringResourceKey& source,
                                                         const EngineeringResourceKey& next) const {
  auto diameter = diameter_;
  auto manufacturing = manufacturing_;
  const auto remap = [&](auto& dimension, bool) {
    if (auto* selected = std::get_if<HoleStandardSelection>(&dimension);
        selected && selected->table == source)
      selected->table = next;
  };
  remap(diameter, false);
  visit_manufacturing_dimensions(manufacturing, remap);
  return create(id_, name_, kind_, target_body_, support_, placements_, std::move(diameter),
                extent_, direction_, tip_, std::move(manufacturing));
}

Result<ResolvedHoleDiameter> resolve_hole_diameter(const PartDocument& part,
                                                   const HoleDiameter& dimension) {
  return resolve_hole_dimension(part, dimension);
}
Result<ResolvedHoleDiameter> resolve_hole_dimension(const PartDocument& part,
                                                    const HoleDimension& diameter, bool angle) {
  const auto fail = [&](std::string message) {
    return Result<ResolvedHoleDiameter>::failure(
        Error::validation(part.id().value(), std::move(message)));
  };
  if (!valid_diameter(diameter))
    return fail("Hole diameter selection is incomplete");
  if (const auto* id = std::get_if<ParameterId>(&diameter)) {
    const auto* p = part.find_parameter(*id);
    if (!p || (angle ? p->type() != ParameterType::Angle || p->value().degrees() <= 0 ||
                           p->value().degrees() >= 180
                     : !positive_length(part, *id)))
      return fail("Hole dimension requires positive Length or an included Angle strictly between 0 "
                  "and 180");
    return Result<ResolvedHoleDiameter>::success({part.find_parameter(*id)->value(), std::nullopt});
  }
  const auto& selection = std::get<HoleStandardSelection>(diameter);
  const auto* table = part.engineering_resources().find_table(selection.table);
  if (!table)
    return fail("Pinned Hole standard table revision is unavailable");
  const auto column =
      std::find_if(table->columns().begin(), table->columns().end(),
                   [&](const auto& c) { return c.key == selection.diameter_column; });
  if (column == table->columns().end() ||
      column->kind !=
          (angle ? EngineeringTableValueKind::Angle : EngineeringTableValueKind::Length))
    return fail(angle ? "Hole included angle must select an Angle standard-table column"
                      : "Hole dimension must select a Length standard-table column");
  const auto* row = table->find_row(selection.row);
  if (!row)
    return fail("Pinned Hole standard row is unavailable");
  const auto cell = std::find_if(row->cells.begin(), row->cells.end(), [&](const auto& c) {
    return c.column == selection.diameter_column;
  });
  if (cell == row->cells.end())
    return fail("Pinned Hole dimension cell is missing");
  const auto* quantity = std::get_if<Quantity>(&cell->value);
  if (!quantity || (angle ? quantity->kind() != QuantityKind::AngleDeg ||
                                quantity->degrees() <= 0 || quantity->degrees() >= 180
                          : !quantity->is_positive_length()))
    return fail(angle ? "Pinned Hole angle cell must be strictly between 0 and 180 degrees"
                      : "Pinned Hole dimension cell must be a positive Length");
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
  std::optional<Error> manufacturing_error;
  visit_manufacturing_dimensions(feature.manufacturing(), [&](const auto& dim, bool angle) {
    auto value = resolve_hole_dimension(part, dim, angle);
    if (!value && !manufacturing_error)
      manufacturing_error = value.error();
  });
  if (manufacturing_error)
    return Result<std::size_t>::failure(*manufacturing_error);
  const double nominal = diameter.value().diameter.millimeters();
  const auto length_value = [&](const auto& dim) {
    return resolve_hole_dimension(part, dim).value().diameter.millimeters();
  };
  const auto seat_valid = [&](const auto& seat) {
    return length_value(seat.diameter) > nominal + 1e-7;
  };
  bool meaningful = std::visit(
      [&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, HoleCounterbore> || std::is_same_v<T, HoleCountersink> ||
                      std::is_same_v<T, HoleSpotface> || std::is_same_v<T, HoleCenter>)
          return seat_valid(value);
        else if constexpr (std::is_same_v<T, HoleStepped>) {
          auto ordered = value.steps;
          std::sort(ordered.begin(), ordered.end(), [&](const auto& a, const auto& b) {
            return length_value(a.depth) < length_value(b.depth);
          });
          double previous_depth = 0, previous_diameter = std::numeric_limits<double>::infinity();
          for (const auto& step : ordered) {
            const double d = length_value(step.diameter), z = length_value(step.depth);
            if (d <= nominal + 1e-7 || d >= previous_diameter - 1e-7 || z <= previous_depth + 1e-7)
              return false;
            previous_depth = z;
            previous_diameter = d;
          }
          return true;
        } else if constexpr (std::is_same_v<T, HoleFitted>)
          return length_value(value.minimum_diameter) <= nominal &&
                 nominal <= length_value(value.maximum_diameter);
        else
          return true;
      },
      feature.manufacturing());
  if (!meaningful)
    return fail(
        "Seat/step diameters and depths or fit limits are inconsistent with the primary bore");
  if (const auto* slot = std::get_if<HoleSlot>(&feature.manufacturing())) {
    auto valid = validate_part_feature_input_reference(part, slot->profile);
    if (!valid)
      return valid;
    const auto* sketch = part.find_sketch(slot->profile.sketch());
    const auto* profile =
        sketch ? sketch->find_arc_closed_profile(slot->profile.profile()) : nullptr;
    if (!profile || profile->curve_segments().size() != 4)
      return fail("Slot Hole requires the existing four-curve arc-closed Sketch profile");
    std::size_t lines = 0, arcs = 0;
    for (const auto& id : profile->curve_segments()) {
      lines += sketch->find_line_segment(id) != nullptr;
      arcs += sketch->find_arc_segment(id) != nullptr;
    }
    if (lines != 2 || arcs != 2)
      return fail("Slot profile must contain two lines and two circular arcs");
  }
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
