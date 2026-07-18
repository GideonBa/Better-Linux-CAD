#include "blcad/gui/gui_interactive_extrude_revolve_binder.hpp"
#include "blcad/gui/gui_interactive_path_sweep_loft.hpp"

#include "blcad/core/body.hpp"
#include "blcad/core/construction_geometry.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/feature.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

using namespace blcad;
using namespace blcad::gui;

namespace {

Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}
Parameter angle(const char* id, double value) {
  return Parameter::create_angle(ParameterId(id), id, Quantity::angle_deg(value, id).value()).value();
}

Result<std::size_t> add_rectangle_sketch(PartDocument& part, const char* sketch_id,
                                         const char* plane_id, const char* profile_id,
                                         const char* width, const char* height,
                                         Point2 center = {}) {
  auto sketch = Sketch::create(SketchId(sketch_id), sketch_id, DatumPlaneId(plane_id)).value();
  if (auto added = sketch.add_profile(RectangleProfile::create(ProfileId(profile_id),
                                                               ParameterId(width),
                                                               ParameterId(height), center).value());
      added.has_error())
    return added;
  return part.add_sketch(std::move(sketch));
}

// A Part with a sweep profile, a bounded construction-line trajectory, two loft
// section sketches on parallel planes, a connected two-line path sketch, and
// result Bodies for sweep and loft.
void build_part(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("part.pathsweeploft"), "PathSweepLoft"));
  REQUIRE(session.commit_part_transaction("Setup", [](PartDocument& part) -> Result<std::size_t> {
    for (auto parameter : {length("sw", 4.0), length("sh", 6.0), length("l1w", 10.0),
                           length("l1h", 8.0), length("l2w", 6.0), length("l2h", 4.0)})
      if (auto added = part.add_parameter(std::move(parameter)); added.has_error()) return added;
    if (auto added = part.add_parameter(angle("twist", 45.0)); added.has_error()) return added;
    if (auto added = part.add_datum_plane(DatumPlane::xy().value()); added.has_error()) return added;
    if (auto added = part.add_construction_plane(
            ConstructionPlane::create_explicit(ConstructionPlaneId("plane.top"), "Top",
                                               {0.0, 0.0, 20.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
                                               {0.0, 0.0, 1.0}).value());
        added.has_error())
      return added;

    // Bounded construction-line trajectory from (0,0,0) to (0,0,20).
    for (auto point : {std::pair{"point.start", Point3{0.0, 0.0, 0.0}},
                       std::pair{"point.end", Point3{0.0, 0.0, 20.0}}})
      if (auto added = part.add_construction_point(
              ConstructionPoint::create_explicit(ConstructionPointId(point.first), point.first,
                                                 point.second).value());
          added.has_error())
        return added;
    auto relation = ConstructionRelation::create_line_through_two_points(
        ConstructionRelationId("relation.path"), ConstructionPointId("point.start"),
        ConstructionPointId("point.end")).value();
    if (auto added = part.add_construction_line(
            ConstructionLine::create_through_two_points(ConstructionLineId("line.path"), "Path",
                                                        relation).value());
        added.has_error())
      return added;
    auto segment = PathSegmentReference::create_construction_line(ConstructionLineId("line.path"));
    if (auto added = part.add_path_curve(
            PathCurve::create(PathCurveId("path.trajectory"), "Trajectory", {segment.value()},
                              PathClosure::Open, PathOrientationRule::MinimumTwist).value());
        added.has_error())
      return added;

    if (auto added = add_rectangle_sketch(part, "sketch.profile", "datum.xy", "profile.sweep", "sw",
                                          "sh");
        added.has_error())
      return added;
    if (auto added = add_rectangle_sketch(part, "sketch.loft1", "datum.xy", "profile.loft1", "l1w",
                                          "l1h");
        added.has_error())
      return added;
    if (auto added = add_rectangle_sketch(part, "sketch.loft2", "plane.top", "profile.loft2", "l2w",
                                          "l2h");
        added.has_error())
      return added;

    // A connected two-line sketch used for interactive PathCurve authoring.
    auto path_sketch = Sketch::create(SketchId("sketch.path"), "Path",
                                      DatumPlaneId("datum.xy")).value();
    if (auto added = path_sketch.add_entity(
            LineSegment::create(SketchEntityId("line.a"), {0.0, 0.0}, {10.0, 0.0}).value());
        added.has_error())
      return added;
    if (auto added = path_sketch.add_entity(
            LineSegment::create(SketchEntityId("line.b"), {10.0, 0.0}, {10.0, 10.0}).value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(path_sketch)); added.has_error()) return added;

    for (const char* body : {"body.sweep", "body.loft"})
      if (auto added = part.add_body(Body::create(BodyId(body), body, BodyKind::Solid).value());
          added.has_error())
        return added;
    return Result<std::size_t>::success(1);
  }));
}

PathSegmentReference planar_segment(const char* entity) {
  return PathSegmentReference::create_planar(SketchId("sketch.path"), SketchEntityId(entity),
                                             PlanarPathCurveKind::Line)
      .value();
}
SweepProfileReference sweep_profile() {
  return SweepProfileReference::create_closed_region(
             ProfileRegionReference::create(SketchId("sketch.profile"), ProfileId("profile.sweep"),
                                            PartFeatureInputRole::SweepProfile).value())
      .value();
}

} // namespace

TEST_CASE("Block 127 interactive PathCurve and Sweep collect segments and commit one transaction",
          "[gui][interactive-path-sweep]") {
  GuiDocumentSession session;
  build_part(session);

  SECTION("path curve collects an ordered connected chain with duplicate rejection") {
    GuiInteractivePathSweepController path;
    REQUIRE(path.begin_path_curve(session, PathCurveId("path.new"), "New Path"));
    REQUIRE(path.add_segment(planar_segment("line.a")));
    CHECK(path.add_segment(planar_segment("line.a")).has_error());  // duplicate
    REQUIRE(path.add_segment(planar_segment("line.b")));
    CHECK(path.segment_count() == 2);
    REQUIRE(path.preview(session));
    const auto before = session.part_document()->path_curves().size();
    REQUIRE(path.apply(session));
    CHECK(session.part_document()->path_curves().size() == before + 1);
  }

  SECTION("sweep authors a solid along the trajectory with exact undo") {
    GuiInteractivePathSweepController sweep;
    REQUIRE(sweep.begin_sweep(session, FeatureId("feature.sweep"), "Sweep", SweepFeatureKind::Sweep,
                              sweep_profile(), PathCurveId("path.trajectory"), BodyId("body.sweep")));
    REQUIRE(sweep.preview(session));
    CHECK(session.part_document()->sweep_features().empty());
    REQUIRE(sweep.apply(session));
    CHECK(session.part_document()->sweep_features().size() == 1);
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.sweep")) != nullptr);
    REQUIRE(session.undo());
    CHECK(session.part_document()->sweep_features().empty());
  }

  SECTION("sweep twist exposes an angle handle and rejects a combined guide") {
    GuiInteractivePathSweepController sweep;
    REQUIRE(sweep.begin_sweep(session, FeatureId("feature.twist"), "Twist", SweepFeatureKind::Sweep,
                              sweep_profile(), PathCurveId("path.trajectory"), BodyId("body.sweep")));
    sweep.set_twist(ParameterId("twist"), 45.0);
    REQUIRE(sweep.handles().size() == 1);
    CHECK(sweep.handles().front().id == std::string(kSweepTwistHandleId));
    REQUIRE(sweep.preview(session));
    sweep.set_guide(PathCurveId("path.trajectory"));
    CHECK(sweep.apply(session).has_error());  // combined guide + twist rejected
  }

  SECTION("sweep fails closed on a missing result body and missing trajectory") {
    GuiInteractivePathSweepController sweep;
    CHECK(sweep.begin_sweep(session, FeatureId("f"), "F", SweepFeatureKind::Sweep, sweep_profile(),
                            PathCurveId("path.trajectory"), BodyId("body.absent")).has_error());
    CHECK(sweep.begin_sweep(session, FeatureId("f"), "F", SweepFeatureKind::Sweep, sweep_profile(),
                            PathCurveId("path.missing"), BodyId("body.sweep")).has_error());
  }
}

TEST_CASE("Block 127 interactive Loft collects ordered sections and commits one transaction",
          "[gui][interactive-loft]") {
  GuiDocumentSession session;
  build_part(session);

  SECTION("loft collects two sections and applies with exact undo") {
    GuiInteractiveLoftController loft;
    REQUIRE(loft.begin_loft(session, FeatureId("feature.loft"), "Loft", LoftFeatureKind::Loft,
                            BodyId("body.loft")));
    REQUIRE(loft.add_closed_section(SketchId("sketch.loft1"), ProfileId("profile.loft1")));
    CHECK(loft.add_closed_section(SketchId("sketch.loft1"), ProfileId("profile.loft1")).has_error());
    REQUIRE(loft.add_closed_section(SketchId("sketch.loft2"), ProfileId("profile.loft2")));
    CHECK(loft.section_count() == 2);
    REQUIRE(loft.preview(session));
    REQUIRE(loft.apply(session));
    CHECK(session.part_document()->loft_features().size() == 1);
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.loft")) != nullptr);
    REQUIRE(session.undo());
    CHECK(session.part_document()->loft_features().empty());
  }

  SECTION("loft fails closed below two sections and on unsupported G2 continuity") {
    GuiInteractiveLoftController loft;
    REQUIRE(loft.begin_loft(session, FeatureId("feature.loft"), "Loft", LoftFeatureKind::Loft,
                            BodyId("body.loft")));
    REQUIRE(loft.add_closed_section(SketchId("sketch.loft1"), ProfileId("profile.loft1")));
    CHECK(loft.preview(session).has_error());  // only one section
    REQUIRE(loft.add_closed_section(SketchId("sketch.loft2"), ProfileId("profile.loft2")));
    loft.set_continuity(LoftContinuity::G2);
    CHECK(loft.apply(session).has_error());  // G2 unsupported without curvature guarantee
  }
}

TEST_CASE("Block 127 ordered loft-section reorder matches a directly ordered loft",
          "[integration][ordered-section-picking]") {
  GuiDocumentSession reordered;
  build_part(reordered);
  GuiInteractiveLoftController controller;
  REQUIRE(controller.begin_loft(reordered, FeatureId("feature.loft"), "Loft", LoftFeatureKind::Loft,
                                BodyId("body.loft")));
  REQUIRE(controller.add_closed_section(SketchId("sketch.loft1"), ProfileId("profile.loft1")));
  REQUIRE(controller.add_closed_section(SketchId("sketch.loft2"), ProfileId("profile.loft2")));
  REQUIRE(controller.reorder_section(0, 1));  // [loft1, loft2] -> [loft2, loft1]
  REQUIRE(controller.apply(reordered));

  GuiDocumentSession typed;
  build_part(typed);
  GuiInteractiveLoftController direct;
  REQUIRE(direct.begin_loft(typed, FeatureId("feature.loft"), "Loft", LoftFeatureKind::Loft,
                            BodyId("body.loft")));
  REQUIRE(direct.add_closed_section(SketchId("sketch.loft2"), ProfileId("profile.loft2")));
  REQUIRE(direct.add_closed_section(SketchId("sketch.loft1"), ProfileId("profile.loft1")));
  REQUIRE(direct.apply(typed));

  CHECK(serialize_part_document_to_json(*reordered.part_document()).value() ==
        serialize_part_document_to_json(*typed.part_document()).value());
}
