#include "blcad/gui/gui_sketch_workbench.hpp"

#include <utility>

namespace blcad::gui {
namespace {

Result<std::size_t> part_only_error(std::string_view operation) {
  return Result<std::size_t>::failure(
      Error::validation("gui.sketch_workbench", std::string(operation) + " requires an active Part document"));
}

const PartDocument* active_part(const GuiDocumentSession& session) { return session.part_document(); }

template <typename Object, typename Add>
Result<std::size_t> create_part_object(GuiDocumentSession& session, std::string label,
                                      Object object, Add add) {
  if (!session.part_document())
    return part_only_error(label);
  return session.commit_part_transaction(std::move(label),
      [object = std::move(object), add](PartDocument& part) mutable {
        return (part.*add)(std::move(object));
      });
}

const Sketch* find_sketch(const GuiDocumentSession& session, SketchId id) {
  const auto* part = active_part(session);
  return part ? part->find_sketch(std::move(id)) : nullptr;
}

double dot(Vector3 a, Vector3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vector3 displacement(Point3 from, Point3 to) noexcept {
  return {to.x - from.x, to.y - from.y, to.z - from.z};
}

} // namespace

Point2 GuiSketchPlaneView::model_to_plane(Point3 point) const noexcept {
  const Vector3 local = displacement(origin, point);
  return {dot(local, x_axis), dot(local, y_axis)};
}

Point3 GuiSketchPlaneView::plane_to_model(Point2 point) const noexcept {
  return {origin.x + point.x * x_axis.x + point.y * y_axis.x,
          origin.y + point.x * x_axis.y + point.y * y_axis.y,
          origin.z + point.x * x_axis.z + point.y * y_axis.z};
}

GuiSketchSelectionPrompt GuiSketchWorkbench::prompt_for(GuiSketchCommand command) {
  switch (command) {
  case GuiSketchCommand::CreateDerivedWorkplane:
    return {"Select a planar generated face", GuiSelectionKind::Face, "planar_face"};
  case GuiSketchCommand::CreateSketch:
    return {"Select a datum plane, construction plane, or derived workplane", GuiSelectionKind::Datum,
            "planar_workplane"};
  case GuiSketchCommand::ProjectReference:
    return {"Select a construction point/line or stable semantic vertex/edge", GuiSelectionKind::Edge,
            "projectable_reference"};
  case GuiSketchCommand::TrimExtend:
    return {"Select an editable planar sketch curve", GuiSelectionKind::SketchEntity,
            "editable_planar_curve"};
  case GuiSketchCommand::AddConstraint:
    return {"Select sketch entities compatible with the chosen constraint", GuiSelectionKind::SketchEntity,
            "constraint_compatible"};
  case GuiSketchCommand::AddDimension:
    return {"Select two compatible sketch points or endpoints", GuiSelectionKind::SketchEntity,
            "dimensionable_points"};
  case GuiSketchCommand::Repair:
  case GuiSketchCommand::Diagnose:
  case GuiSketchCommand::InspectProfiles:
    return {"Select one planar sketch", GuiSelectionKind::SketchEntity, "planar_sketch"};
  case GuiSketchCommand::CreateDatumPlane:
  case GuiSketchCommand::CreateDatumAxis:
  case GuiSketchCommand::AddConstructionGeometry:
  case GuiSketchCommand::AddLine:
  case GuiSketchCommand::AddArcCircle:
  case GuiSketchCommand::AddSpline:
    return {"Enter deterministic numeric intent in sketch-plane coordinates",
            GuiSelectionKind::SketchEntity, "active_sketch"};
  }
  return {"Select compatible geometry", GuiSelectionKind::SketchEntity, "compatible_geometry"};
}

Result<std::size_t> GuiSketchWorkbench::create_xy_datum(GuiDocumentSession& session, DatumPlaneId id,
                                                        std::string name) const {
  auto datum = DatumPlane::xy(std::move(id), std::move(name));
  if (datum.has_error()) return Result<std::size_t>::failure(datum.error());
  return create_part_object(session, "Create datum plane", std::move(datum.value()),
                            &PartDocument::add_datum_plane);
}

Result<std::size_t> GuiSketchWorkbench::create_datum_axis(GuiDocumentSession& session,
                                                          DatumAxis axis) const {
  return create_part_object(session, "Create datum axis", std::move(axis), &PartDocument::add_datum_axis);
}
Result<std::size_t> GuiSketchWorkbench::create_construction_point(GuiDocumentSession& session,
                                                                  ConstructionPoint point) const {
  return create_part_object(session, "Create construction point", std::move(point), &PartDocument::add_construction_point);
}
Result<std::size_t> GuiSketchWorkbench::create_construction_line(GuiDocumentSession& session,
                                                                 ConstructionLine line) const {
  return create_part_object(session, "Create construction line", std::move(line), &PartDocument::add_construction_line);
}
Result<std::size_t> GuiSketchWorkbench::create_construction_plane(GuiDocumentSession& session,
                                                                  ConstructionPlane plane) const {
  return create_part_object(session, "Create construction plane", std::move(plane), &PartDocument::add_construction_plane);
}
Result<std::size_t> GuiSketchWorkbench::create_derived_workplane(GuiDocumentSession& session,
                                                                 DerivedWorkplane workplane) const {
  return create_part_object(session, "Create derived workplane", std::move(workplane), &PartDocument::add_derived_workplane);
}
Result<std::size_t> GuiSketchWorkbench::create_sketch(GuiDocumentSession& session, Sketch sketch) const {
  return create_part_object(session, "Create sketch", std::move(sketch), &PartDocument::add_sketch);
}

Result<std::size_t> GuiSketchWorkbench::edit_sketch(GuiDocumentSession& session, SketchId sketch,
                                                    std::string label,
                                                    const GuiSketchMutation& mutation) const {
  if (!session.part_document()) return part_only_error(label);
  return session.commit_part_transaction(std::move(label), [sketch, &mutation](PartDocument& part) {
    const auto* existing = part.find_sketch(sketch);
    if (!existing)
      return Result<std::size_t>::failure(Error::validation(sketch.value(), "sketch must exist"));
    Sketch candidate = *existing;
    auto edited = mutation(candidate);
    if (edited.has_error()) return Result<std::size_t>::failure(edited.error());
    return part.update_sketch(std::move(candidate));
  });
}

#define BLCAD_SKETCH_ADD_METHOD(NAME, TYPE, METHOD, LABEL)                                      \
  Result<std::size_t> GuiSketchWorkbench::NAME(GuiDocumentSession& session, SketchId sketch,     \
                                                TYPE object) const {                              \
    return edit_sketch(session, std::move(sketch), LABEL,                                        \
                       [object = std::move(object)](Sketch& candidate) mutable {                   \
                         return candidate.METHOD(std::move(object));                              \
                       });                                                                        \
  }

BLCAD_SKETCH_ADD_METHOD(add_line, LineSegment, add_entity, "Add sketch line")
BLCAD_SKETCH_ADD_METHOD(add_arc, ArcSegment, add_entity, "Add sketch arc")
BLCAD_SKETCH_ADD_METHOD(add_spline, SplineSegment, add_entity, "Add sketch spline")
BLCAD_SKETCH_ADD_METHOD(add_trim_extend, SketchTrimExtendOperation, add_trim_extend_operation,
                        "Trim or extend sketch curve")
BLCAD_SKETCH_ADD_METHOD(add_projected_point, ProjectedSketchPoint, add_reference,
                        "Project sketch point reference")
BLCAD_SKETCH_ADD_METHOD(add_projected_line, ProjectedSketchLine, add_reference,
                        "Project sketch line reference")
BLCAD_SKETCH_ADD_METHOD(add_reference_line, ReferenceGeneratedLine, add_reference,
                        "Create reference-driven sketch line")
BLCAD_SKETCH_ADD_METHOD(add_constraint, SketchConstraint, add_constraint, "Add sketch reference constraint")
BLCAD_SKETCH_ADD_METHOD(add_constraint, SketchGeometricConstraint, add_constraint, "Add sketch geometric constraint")
BLCAD_SKETCH_ADD_METHOD(add_dimension, SketchDrivingDimension, add_dimension, "Add sketch dimension")
BLCAD_SKETCH_ADD_METHOD(add_tangent_continuity, SketchTangentContinuity, add_tangent_continuity,
                        "Add sketch tangent continuity")
BLCAD_SKETCH_ADD_METHOD(add_profile, ClosedProfile, add_profile, "Add closed sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, ArcClosedProfile, add_profile, "Add curved sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, CompositeClosedProfile, add_profile, "Add composite sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, CircleProfile, add_profile, "Add circle sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, RectangleProfile, add_profile, "Add rectangle sketch profile")
BLCAD_SKETCH_ADD_METHOD(add_profile, CircularHolePattern, add_profile, "Add circular hole pattern")

#undef BLCAD_SKETCH_ADD_METHOD

Result<GuiSketchPlaneView> GuiSketchWorkbench::plane_view(const GuiDocumentSession& session,
                                                          SketchId sketch) const {
  const auto* part = active_part(session);
  const auto* target = part ? part->find_sketch(std::move(sketch)) : nullptr;
  if (!target)
    return Result<GuiSketchPlaneView>::failure(Error::validation("gui.sketch_workbench", "sketch must exist"));
  geometry::WorkplaneResolver resolver;
  auto plane = session.part_shape_cache()
                   ? resolver.resolve_for_sketch(*part, *target, *session.part_shape_cache())
                   : resolver.resolve_for_sketch(*part, *target);
  if (plane.has_error()) return Result<GuiSketchPlaneView>::failure(plane.error());
  return Result<GuiSketchPlaneView>::success({plane.value().id, plane.value().origin,
                                              plane.value().x_axis, plane.value().y_axis,
                                              plane.value().normal});
}

Result<GuiSketchInspection> GuiSketchWorkbench::inspect(const GuiDocumentSession& session,
                                                        SketchId sketch) const {
  const auto* target = find_sketch(session, sketch);
  if (!target)
    return Result<GuiSketchInspection>::failure(Error::validation(sketch.value(), "sketch must exist"));
  SketchConstraintDiagnostics analyzer;
  auto diagnostics = analyzer.analyze(*target);
  SketchRepairSuggester suggester;
  auto repairs = suggester.suggest(diagnostics);
  std::optional<ProfileId> detected_region;
  std::string region_message = "no explicit closed region requested";
  if (target->line_segments().size() >= 3U) {
    auto region = geometry::SketchRegionFinder{}.find_single_region(*active_part(session), *target);
    if (region.has_error())
      region_message = region.error().message();
    else {
      detected_region = region.value().id;
      region_message = "one deterministic closed region detected";
    }
  }
  GuiSketchInspection result{target->id(),
      target->line_segments().size() + target->arc_segments().size() + target->spline_segments().size(),
      target->projected_points().size() + target->projected_lines().size() + target->reference_generated_lines().size(),
      target->constraints().size() + target->geometric_constraints().size() + target->tangent_continuities().size(),
      target->driving_dimensions().size(), target->profile_count(), std::move(detected_region),
      std::move(region_message), std::move(diagnostics), std::move(repairs)};
  return Result<GuiSketchInspection>::success(std::move(result));
}

Result<SketchRepairCommandResult>
GuiSketchWorkbench::preview_repair(const GuiDocumentSession& session, SketchId sketch,
                                   const SketchRepairSuggestion& suggestion) const {
  const auto* target = find_sketch(session, std::move(sketch));
  if (!target)
    return Result<SketchRepairCommandResult>::failure(Error::validation("gui.sketch_repair", "sketch must exist"));
  Sketch candidate = *target;
  return SketchRepairCommandExecutor{}.apply(candidate, SketchRepairCommand(suggestion));
}

Result<std::size_t> GuiSketchWorkbench::apply_repair(GuiDocumentSession& session, SketchId sketch,
                                                     const SketchRepairSuggestion& suggestion) const {
  const auto preview = preview_repair(session, sketch, suggestion);
  if (preview.has_error()) return Result<std::size_t>::failure(preview.error());
  if (!preview.value().applied())
    return Result<std::size_t>::failure(Error::validation(sketch.value(), preview.value().message()));
  return edit_sketch(session, std::move(sketch), "Repair sketch",
      [suggestion](Sketch& candidate) {
        auto applied = SketchRepairCommandExecutor{}.apply(candidate, SketchRepairCommand(suggestion));
        if (applied.has_error()) return Result<std::size_t>::failure(applied.error());
        return applied.value().applied()
                   ? Result<std::size_t>::success(applied.value().changed_constraint_ids().size() +
                                                  applied.value().changed_dimension_ids().size())
                   : Result<std::size_t>::failure(Error::validation(candidate.id().value(), applied.value().message()));
      });
}

} // namespace blcad::gui
