#pragma once

#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch_edit_commands.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace blcad {

class SketchTopologyLegacyMaterializer {
public:
  [[nodiscard]] Result<Sketch> materialize(const Sketch& source,
                                           const SketchTopology& topology) const {
    if (source.id() != topology.sketch())
      return Result<Sketch>::failure(Error::validation(
          source.id().value(), "Sketch topology belongs to a different Sketch"));

    auto result = Sketch::create(source.id(), source.name(), source.workplane());
    if (result.has_error()) return result;

    const auto entity = [&](std::string_view family, const SketchEntityId& id,
                            SketchTopologyEntityKind expected)
        -> Result<const SketchTopologyEntity*> {
      const std::string topology_id = std::string(family) + id.value();
      const auto* found = topology.find_entity(topology_id);
      if (found == nullptr)
        return Result<const SketchTopologyEntity*>::failure(
            Error::validation(topology_id, "Sketch topology is missing a legacy entity"));
      if (found->kind() != expected)
        return Result<const SketchTopologyEntity*>::failure(
            Error::validation(topology_id, "Sketch topology entity kind does not match legacy entity"));
      return Result<const SketchTopologyEntity*>::success(found);
    };
    const auto profile_entity = [&](const ProfileId& id, SketchTopologyEntityKind expected)
        -> Result<const SketchTopologyEntity*> {
      const std::string topology_id = "profile/" + id.value();
      const auto* found = topology.find_entity(topology_id);
      if (found == nullptr)
        return Result<const SketchTopologyEntity*>::failure(
            Error::validation(topology_id, "Sketch topology is missing a legacy profile"));
      if (found->kind() != expected)
        return Result<const SketchTopologyEntity*>::failure(
            Error::validation(topology_id, "Sketch topology profile kind does not match legacy profile"));
      return Result<const SketchTopologyEntity*>::success(found);
    };
    const auto point = [&](const SketchTopologyEntity& owner, std::size_t index)
        -> Result<Point2> {
      if (index >= owner.points().size())
        return Result<Point2>::failure(
            Error::validation(owner.id(), "Sketch topology entity point index is out of range"));
      const auto* found = topology.find_point(owner.points()[index]);
      if (found == nullptr)
        return Result<Point2>::failure(
            Error::validation(owner.id(), "Sketch topology entity references an unknown point"));
      return Result<Point2>::success(found->position());
    };

    for (const auto& legacy : source.line_segments()) {
      auto current = entity("entity/", legacy.id(), SketchTopologyEntityKind::Line);
      if (current.has_error()) return Result<Sketch>::failure(current.error());
      auto start = point(*current.value(), 0U);
      auto end = point(*current.value(), 1U);
      if (start.has_error()) return Result<Sketch>::failure(start.error());
      if (end.has_error()) return Result<Sketch>::failure(end.error());
      auto line = LineSegment::create(legacy.id(), start.value(), end.value());
      if (line.has_error()) return Result<Sketch>::failure(line.error());
      auto added = result.value().add_entity(std::move(line.value()));
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& legacy : source.arc_segments()) {
      auto current = entity("entity/", legacy.id(), SketchTopologyEntityKind::Arc);
      if (current.has_error()) return Result<Sketch>::failure(current.error());
      auto start = point(*current.value(), 0U);
      auto mid = point(*current.value(), 1U);
      auto end = point(*current.value(), 2U);
      if (start.has_error()) return Result<Sketch>::failure(start.error());
      if (mid.has_error()) return Result<Sketch>::failure(mid.error());
      if (end.has_error()) return Result<Sketch>::failure(end.error());
      auto arc = ArcSegment::create_three_point(legacy.id(), start.value(), mid.value(), end.value());
      if (arc.has_error()) return Result<Sketch>::failure(arc.error());
      auto added = result.value().add_entity(std::move(arc.value()));
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& legacy : source.spline_segments()) {
      auto current = entity("entity/", legacy.id(), SketchTopologyEntityKind::Spline);
      if (current.has_error()) return Result<Sketch>::failure(current.error());
      auto start = point(*current.value(), 0U);
      auto control1 = point(*current.value(), 1U);
      auto control2 = point(*current.value(), 2U);
      auto end = point(*current.value(), 3U);
      if (start.has_error()) return Result<Sketch>::failure(start.error());
      if (control1.has_error()) return Result<Sketch>::failure(control1.error());
      if (control2.has_error()) return Result<Sketch>::failure(control2.error());
      if (end.has_error()) return Result<Sketch>::failure(end.error());
      auto spline = SplineSegment::create_cubic_bezier(
          legacy.id(), start.value(), control1.value(), control2.value(), end.value());
      if (spline.has_error()) return Result<Sketch>::failure(spline.error());
      auto added = result.value().add_entity(std::move(spline.value()));
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }

    for (const auto& reference : source.projected_points()) {
      auto added = result.value().add_reference(reference);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& reference : source.projected_lines()) {
      auto added = result.value().add_reference(reference);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& reference : source.reference_generated_lines()) {
      auto added = result.value().add_reference(reference);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }

    for (const auto& constraint : source.constraints()) {
      auto added = result.value().add_constraint(constraint);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& constraint : source.geometric_constraints()) {
      auto added = result.value().add_constraint(constraint);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& dimension : source.driving_dimensions()) {
      auto added = result.value().add_dimension(dimension);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& operation : source.trim_extend_operations()) {
      auto added = result.value().add_trim_extend_operation(operation);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& continuity : source.tangent_continuities()) {
      auto added = result.value().add_tangent_continuity(continuity);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }

    for (const auto& legacy : source.rectangle_profiles()) {
      auto current = profile_entity(legacy.id(), SketchTopologyEntityKind::RectangleProfile);
      if (current.has_error()) return Result<Sketch>::failure(current.error());
      auto center = point(*current.value(), 0U);
      if (center.has_error()) return Result<Sketch>::failure(center.error());
      auto profile = RectangleProfile::create(legacy.id(), legacy.width_parameter(),
                                              legacy.height_parameter(), center.value());
      if (profile.has_error()) return Result<Sketch>::failure(profile.error());
      auto added = result.value().add_profile(std::move(profile.value()));
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& legacy : source.circle_profiles()) {
      auto current = profile_entity(legacy.id(), SketchTopologyEntityKind::CircleProfile);
      if (current.has_error()) return Result<Sketch>::failure(current.error());
      auto center = point(*current.value(), 0U);
      if (center.has_error()) return Result<Sketch>::failure(center.error());
      auto profile = CircleProfile::create(legacy.id(), legacy.diameter_parameter(), center.value());
      if (profile.has_error()) return Result<Sketch>::failure(profile.error());
      auto added = result.value().add_profile(std::move(profile.value()));
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& legacy : source.closed_profiles()) {
      auto current = profile_entity(legacy.id(), SketchTopologyEntityKind::ClosedProfile);
      if (current.has_error()) return Result<Sketch>::failure(current.error());
      std::vector<SketchEntityId> curve_ids;
      for (const auto& dependency : current.value()->entity_dependencies()) {
        constexpr std::string_view prefix = "entity/";
        if (!std::string_view(dependency).starts_with(prefix))
          return Result<Sketch>::failure(Error::validation(
              current.value()->id(), "closed profile topology dependency is not a Sketch entity"));
        curve_ids.emplace_back(dependency.substr(prefix.size()));
      }
      auto profile = ClosedProfile::create(legacy.id(), std::move(curve_ids));
      if (profile.has_error()) return Result<Sketch>::failure(profile.error());
      auto added = result.value().add_profile(std::move(profile.value()));
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& legacy : source.arc_closed_profiles()) {
      auto current = profile_entity(legacy.id(), SketchTopologyEntityKind::ArcClosedProfile);
      if (current.has_error()) return Result<Sketch>::failure(current.error());
      std::vector<SketchEntityId> curve_ids;
      for (const auto& dependency : current.value()->entity_dependencies()) {
        constexpr std::string_view prefix = "entity/";
        if (!std::string_view(dependency).starts_with(prefix))
          return Result<Sketch>::failure(Error::validation(
              current.value()->id(), "arc profile topology dependency is not a Sketch entity"));
        curve_ids.emplace_back(dependency.substr(prefix.size()));
      }
      auto profile = ArcClosedProfile::create(legacy.id(), std::move(curve_ids));
      if (profile.has_error()) return Result<Sketch>::failure(profile.error());
      auto added = result.value().add_profile(std::move(profile.value()));
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& legacy : source.composite_closed_profiles()) {
      auto current = profile_entity(legacy.id(), SketchTopologyEntityKind::CompositeClosedProfile);
      if (current.has_error()) return Result<Sketch>::failure(current.error());
      if (current.value()->entity_dependencies() != [&legacy] {
            std::vector<std::string> ids;
            for (const auto& curve : legacy.outer_contour()) ids.push_back("entity/" + curve.value());
            for (const auto& contour : legacy.inner_contours())
              for (const auto& curve : contour) ids.push_back("entity/" + curve.value());
            return ids;
          }())
        return Result<Sketch>::failure(Error::validation(
            current.value()->id(),
            "composite profile contour partition cannot be changed before Block 116"));
      auto added = result.value().add_profile(legacy);
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }
    for (const auto& legacy : source.circular_hole_patterns()) {
      auto current = profile_entity(legacy.id(), SketchTopologyEntityKind::CircularHolePattern);
      if (current.has_error()) return Result<Sketch>::failure(current.error());
      auto center = point(*current.value(), 0U);
      if (center.has_error()) return Result<Sketch>::failure(center.error());
      auto pattern = CircularHolePattern::create(
          legacy.id(), legacy.radius_parameter(), legacy.count_parameter(),
          legacy.hole_diameter_parameter(), center.value(), legacy.angle_offset_deg());
      if (pattern.has_error()) return Result<Sketch>::failure(pattern.error());
      auto added = result.value().add_profile(std::move(pattern.value()));
      if (added.has_error()) return Result<Sketch>::failure(added.error());
    }

    return result;
  }
};

struct SketchTopologyPartEditResult {
  SketchTopologyMigrationReport migration;
  SketchEditTransaction transaction;
};

class SketchTopologyPartDocumentEditor {
public:
  [[nodiscard]] Result<SketchTopologyPartEditResult>
  apply(PartDocument& document, SketchId sketch_id, const SketchEditCommand& command) const {
    const Sketch* source = document.find_sketch(sketch_id);
    if (source == nullptr)
      return Result<SketchTopologyPartEditResult>::failure(Error::validation(
          sketch_id.empty() ? "sketch_topology_edit" : sketch_id.value(),
          "Part document does not contain the requested Sketch"));

    SketchTopologyMigrationReport migration;
    auto topology = SketchTopology::migrate_legacy(*source, &migration);
    if (topology.has_error())
      return Result<SketchTopologyPartEditResult>::failure(topology.error());
    auto transaction = SketchEditCommandExecutor{}.apply(topology.value(), command);
    if (transaction.has_error())
      return Result<SketchTopologyPartEditResult>::failure(transaction.error());
    auto materialized = SketchTopologyLegacyMaterializer{}.materialize(
        *source, transaction.value().after());
    if (materialized.has_error())
      return Result<SketchTopologyPartEditResult>::failure(materialized.error());
    auto updated = document.update_sketch(std::move(materialized.value()));
    if (updated.has_error())
      return Result<SketchTopologyPartEditResult>::failure(updated.error());

    return Result<SketchTopologyPartEditResult>::success(
        {std::move(migration), std::move(transaction.value())});
  }
};

} // namespace blcad
