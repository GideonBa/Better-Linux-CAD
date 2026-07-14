#pragma once

#include "blcad/core/sketch_diagnostics.hpp"
#include "blcad/core/sketch_repair_commands.hpp"
#include "blcad/core/sketch_repair_suggestions.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/geometry/sketch_region_finder.hpp"
#include "blcad/gui/gui_document_session.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blcad::gui {

enum class GuiSketchCommand {
  CreateDatumPlane,
  CreateDatumAxis,
  CreateDerivedWorkplane,
  CreateSketch,
  AddConstructionGeometry,
  AddLine,
  AddArcCircle,
  AddSpline,
  TrimExtend,
  ProjectReference,
  AddConstraint,
  AddDimension,
  InspectProfiles,
  Diagnose,
  Repair,
};

struct GuiSketchSelectionPrompt {
  std::string text;
  GuiSelectionKind required_kind{GuiSelectionKind::Datum};
  std::string required_capability;
};

struct GuiSketchPlaneView {
  DatumPlaneId workplane;
  Point3 origin;
  Vector3 x_axis;
  Vector3 y_axis;
  Vector3 normal;

  [[nodiscard]] Point2 model_to_plane(Point3 point) const noexcept;
  [[nodiscard]] Point3 plane_to_model(Point2 point) const noexcept;
};

struct GuiSketchInspection {
  SketchId sketch;
  std::size_t explicit_entity_count{0};
  std::size_t projected_reference_count{0};
  std::size_t constraint_count{0};
  std::size_t dimension_count{0};
  std::size_t profile_count{0};
  std::optional<ProfileId> detected_region;
  std::string region_message;
  SketchDiagnosticReport diagnostics{SketchId("sketch.inspection")};
  SketchRepairSuggestionReport repairs{SketchId("sketch.inspection")};
};

using GuiSketchMutation = std::function<Result<std::size_t>(Sketch&)>;

class GuiSketchWorkbench {
public:
  [[nodiscard]] static GuiSketchSelectionPrompt prompt_for(GuiSketchCommand command);

  [[nodiscard]] Result<std::size_t> create_xy_datum(GuiDocumentSession& session, DatumPlaneId id,
                                                    std::string name) const;
  [[nodiscard]] Result<std::size_t> create_datum_axis(GuiDocumentSession& session, DatumAxis axis) const;
  [[nodiscard]] Result<std::size_t> create_construction_point(GuiDocumentSession& session,
                                                               ConstructionPoint point) const;
  [[nodiscard]] Result<std::size_t> create_construction_line(GuiDocumentSession& session,
                                                              ConstructionLine line) const;
  [[nodiscard]] Result<std::size_t> create_construction_plane(GuiDocumentSession& session,
                                                               ConstructionPlane plane) const;
  [[nodiscard]] Result<std::size_t> create_derived_workplane(GuiDocumentSession& session,
                                                              DerivedWorkplane workplane) const;
  [[nodiscard]] Result<std::size_t> create_sketch(GuiDocumentSession& session, Sketch sketch) const;

  [[nodiscard]] Result<std::size_t> edit_sketch(GuiDocumentSession& session, SketchId sketch,
                                                 std::string label,
                                                 const GuiSketchMutation& mutation) const;
  [[nodiscard]] Result<std::size_t> add_line(GuiDocumentSession& session, SketchId sketch,
                                             LineSegment line) const;
  [[nodiscard]] Result<std::size_t> add_arc(GuiDocumentSession& session, SketchId sketch,
                                            ArcSegment arc) const;
  [[nodiscard]] Result<std::size_t> add_spline(GuiDocumentSession& session, SketchId sketch,
                                               SplineSegment spline) const;
  [[nodiscard]] Result<std::size_t> add_trim_extend(GuiDocumentSession& session, SketchId sketch,
                                                    SketchTrimExtendOperation operation) const;
  [[nodiscard]] Result<std::size_t> add_projected_point(GuiDocumentSession& session, SketchId sketch,
                                                        ProjectedSketchPoint point) const;
  [[nodiscard]] Result<std::size_t> add_projected_line(GuiDocumentSession& session, SketchId sketch,
                                                       ProjectedSketchLine line) const;
  [[nodiscard]] Result<std::size_t> add_reference_line(GuiDocumentSession& session, SketchId sketch,
                                                       ReferenceGeneratedLine line) const;
  [[nodiscard]] Result<std::size_t> add_constraint(GuiDocumentSession& session, SketchId sketch,
                                                   SketchConstraint constraint) const;
  [[nodiscard]] Result<std::size_t> add_constraint(GuiDocumentSession& session, SketchId sketch,
                                                   SketchGeometricConstraint constraint) const;
  [[nodiscard]] Result<std::size_t> add_dimension(GuiDocumentSession& session, SketchId sketch,
                                                  SketchDrivingDimension dimension) const;
  [[nodiscard]] Result<std::size_t> add_tangent_continuity(
      GuiDocumentSession& session, SketchId sketch, SketchTangentContinuity continuity) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                ClosedProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                ArcClosedProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                CompositeClosedProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                CircleProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                RectangleProfile profile) const;
  [[nodiscard]] Result<std::size_t> add_profile(GuiDocumentSession& session, SketchId sketch,
                                                CircularHolePattern pattern) const;

  [[nodiscard]] Result<GuiSketchPlaneView> plane_view(const GuiDocumentSession& session,
                                                       SketchId sketch) const;
  [[nodiscard]] Result<GuiSketchInspection> inspect(const GuiDocumentSession& session,
                                                     SketchId sketch) const;
  [[nodiscard]] Result<SketchRepairCommandResult>
  preview_repair(const GuiDocumentSession& session, SketchId sketch,
                 const SketchRepairSuggestion& suggestion) const;
  [[nodiscard]] Result<std::size_t> apply_repair(GuiDocumentSession& session, SketchId sketch,
                                                 const SketchRepairSuggestion& suggestion) const;
};

} // namespace blcad::gui
