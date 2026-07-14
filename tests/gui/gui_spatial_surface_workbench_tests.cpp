#include "blcad/gui/gui_part_foundation_workbench.hpp"
#include "blcad/gui/gui_sketch_workbench.hpp"
#include "blcad/gui/gui_spatial_surface_workbench.hpp"

#include "blcad/geometry/viewport_scene.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace blcad;
using namespace blcad::gui;

namespace {
Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}
SketchCoordinate3D coordinate(double value) {
  return SketchCoordinate3D::create_explicit(
      Quantity::linear_displacement_mm(value, "3d-coordinate").value()).value();
}
SketchPoint3D point(const char* id, Point3 value) {
  return SketchPoint3D::create(SketchEntityId(id), coordinate(value.x), coordinate(value.y),
                               coordinate(value.z)).value();
}
FeatureBodyResultContext new_body(const char* id) {
  return FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                          BodyId(id)).value();
}
} // namespace

TEST_CASE("Block 102 GUI builds a 3D path solid Sweep and Fill Surface",
          "[gui][path-workbench][gui][sweep-loft][gui][surface-workbench]") {
  GuiDocumentSession session;
  GuiPartFoundationWorkbench foundation;
  GuiSketchWorkbench sketches;
  GuiSpatialSurfaceWorkbench spatial;
  REQUIRE(session.create_part(DocumentId("part.spatial"), "Spatial"));
  REQUIRE(foundation.create_parameter(session, length("profile.width", 4)));
  REQUIRE(foundation.create_parameter(session, length("profile.height", 6)));
  REQUIRE(sketches.create_xy_datum(session, DatumPlaneId("datum.xy"), "XY"));

  auto profile = Sketch::create(SketchId("sketch.profile"), "Profile", DatumPlaneId("datum.xy")).value();
  REQUIRE(profile.add_profile(RectangleProfile::create(ProfileId("profile.sweep"),
      ParameterId("profile.width"), ParameterId("profile.height")).value()));
  REQUIRE(sketches.create_sketch(session, std::move(profile)));

  auto sketch3d = Sketch3D::create(Sketch3DId("sketch3d.path"), "Spatial path").value();
  REQUIRE(sketch3d.add_point(point("point.start", {0, 0, 0})));
  REQUIRE(sketch3d.add_point(point("point.end", {0, 0, 25})));
  REQUIRE(sketch3d.add_line(SketchLine3D::create(SketchEntityId("line.path"),
      SketchEntityId("point.start"), SketchEntityId("point.end")).value()));
  REQUIRE(sketch3d.add_guide_curve(SketchGuideCurve3D::create(SketchEntityId("guide.path"),
      SketchEntityId("line.path"), SketchGuideCurve3DRole::SweepPath, "Sweep path").value()));
  REQUIRE(spatial.create_sketch_3d(session, std::move(sketch3d)));
  auto segment = PathSegmentReference::create_sketch_3d(Sketch3DId("sketch3d.path"),
      SketchEntityId("line.path"), Sketch3DPathCurveKind::Line).value();
  REQUIRE(spatial.create_path_curve(session, PathCurve::create(PathCurveId("path.sweep"), "Sweep",
      {segment}, PathClosure::Open, PathOrientationRule::MinimumTwist).value()));
  REQUIRE(foundation.create_body(session, Body::create(BodyId("body.sweep"), "Sweep",
                                                        BodyKind::Solid).value()));
  auto region = ProfileRegionReference::create(SketchId("sketch.profile"), ProfileId("profile.sweep"),
                                                PartFeatureInputRole::SweepProfile).value();
  auto sweep = SweepFeature::create_sweep(FeatureId("sweep.main"), "Sweep",
      SweepProfileReference::create_closed_region(region).value(), PathCurveId("path.sweep"),
      new_body("body.sweep")).value();
  auto sweep_preview = spatial.preview_sweep(session, sweep);
  REQUIRE(sweep_preview);
  CHECK(sweep_preview.value().output_kind == BodyKind::Solid);
  CHECK(session.part_document()->sweep_features().empty());
  REQUIRE(spatial.apply_sweep(session, sweep));

  auto boundary_sketch = Sketch::create(SketchId("sketch.boundary"), "Boundary",
                                         DatumPlaneId("datum.xy")).value();
  const std::vector<std::pair<Point2, Point2>> lines = {{{10, 0}, {20, 0}}, {{20, 0}, {20, 8}},
      {{20, 8}, {10, 8}}, {{10, 8}, {10, 0}}};
  std::vector<PathSegmentReference> boundary_segments;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const SketchEntityId id("boundary." + std::to_string(i));
    REQUIRE(boundary_sketch.add_entity(LineSegment::create(id, lines[i].first, lines[i].second).value()));
    boundary_segments.push_back(PathSegmentReference::create_planar(
        SketchId("sketch.boundary"), id, PlanarPathCurveKind::Line).value());
  }
  REQUIRE(sketches.create_sketch(session, std::move(boundary_sketch)));
  REQUIRE(spatial.create_path_curve(session, PathCurve::create(PathCurveId("path.boundary"),
      "Boundary", std::move(boundary_segments), PathClosure::Closed,
      PathOrientationRule::MinimumTwist).value()));
  REQUIRE(foundation.create_body(session, Body::create(BodyId("body.surface"), "Surface",
                                                        BodyKind::Surface).value()));
  auto boundary = BoundaryCurveReference::create_path_curve(PathCurveId("path.boundary")).value();
  SurfaceFeature fill = FillSurfaceFeature::create(FeatureId("surface.fill"), "Fill", {boundary},
                                                    BodyId("body.surface")).value();
  auto fill_preview = spatial.preview_surface(session, fill);
  REQUIRE(fill_preview);
  CHECK(fill_preview.value().output_kind == BodyKind::Surface);
  REQUIRE(spatial.apply_surface(session, fill));
  REQUIRE(session.recompute());

  auto scene = geometry::ViewportSceneBuilder{}.build_part(*session.part_document(),
                                                            *session.part_shape_cache());
  REQUIRE(scene);
  CHECK(std::ranges::any_of(scene.value(), [](const auto& item) {
    return item.kind == geometry::ViewportSceneKind::SolidBody;
  }));
  CHECK(std::ranges::any_of(scene.value(), [](const auto& item) {
    return item.kind == geometry::ViewportSceneKind::SurfaceBody;
  }));
  CHECK(std::ranges::any_of(scene.value(), [](const auto& item) {
    return item.kind == geometry::ViewportSceneKind::Path;
  }));
}

TEST_CASE("Block 102 preview preserves ordered Loft inputs and fails closed",
          "[gui][sweep-loft][gui][surface-workbench]") {
  GuiDocumentSession session;
  GuiSpatialSurfaceWorkbench spatial;
  REQUIRE(session.create_part(DocumentId("part.invalid-loft"), "Invalid Loft"));
  REQUIRE(session.commit_part_transaction("Create result Body", [](PartDocument& part) {
    return part.add_body(Body::create(BodyId("body.loft"), "Loft", BodyKind::Solid).value());
  }));
  auto first = ProfileSectionReference::create_closed_region(
      ProfileRegionReference::create(SketchId("missing.a"), ProfileId("profile.a"),
                                     PartFeatureInputRole::LoftSection).value()).value();
  auto second = ProfileSectionReference::create_closed_region(
      ProfileRegionReference::create(SketchId("missing.b"), ProfileId("profile.b"),
                                     PartFeatureInputRole::LoftSection).value()).value();
  auto loft = LoftFeature::create_loft(FeatureId("loft.invalid"), "Invalid", {first, second},
                                       new_body("body.loft")).value();
  CHECK(spatial.preview_loft(session, loft).has_error());
  CHECK(session.part_document()->loft_features().empty());
}

TEST_CASE("Block 102 prompts distinguish spatial and Surface capabilities",
          "[gui][path-workbench][gui][sweep-loft][gui][surface-workbench]") {
  CHECK(GuiSpatialSurfaceWorkbench::prompt_for(GuiSpatialCommand::PathCurve).required_capability ==
        "connected_ordered_path");
  CHECK(GuiSpatialSurfaceWorkbench::prompt_for(GuiSpatialCommand::Loft).text.find("ordered") !=
        std::string::npos);
  CHECK(GuiSpatialSurfaceWorkbench::prompt_for(GuiSpatialCommand::ClosedShellToSolid)
            .required_capability == "closed_shell");
}
