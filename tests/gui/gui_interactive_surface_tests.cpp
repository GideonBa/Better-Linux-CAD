#include "blcad/gui/gui_interactive_extrude_revolve_binder.hpp"
#include "blcad/gui/gui_interactive_surface.hpp"

#include "blcad/core/body.hpp"
#include "blcad/core/parameter.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/path_curve.hpp"
#include "blcad/core/quantity.hpp"
#include "blcad/core/sketch_3d.hpp"
#include "blcad/core/surface_feature.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace blcad;
using namespace blcad::gui;

namespace {

using Corner = std::array<double, 3>;
using Quad = std::array<Corner, 4>;

Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}
SketchCoordinate3D coordinate(double value) {
  return SketchCoordinate3D::create_explicit(
             Quantity::linear_displacement_mm(value, "surface coordinate").value())
      .value();
}

// Adds a Sketch3D quad plus a closed boundary PathCurve "path.<tag>" and an open
// single-edge PathCurve "edge.<tag>" (the first side), returning nothing.
Result<std::size_t> add_quad(PartDocument& part, const std::string& tag, const Quad& corners) {
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d." + tag), tag).value();
  for (std::size_t i = 0; i < 4; ++i)
    if (auto added = sketch.add_point(
            SketchPoint3D::create(SketchEntityId(tag + ".p" + std::to_string(i)),
                                  coordinate(corners[i][0]), coordinate(corners[i][1]),
                                  coordinate(corners[i][2])).value());
        added.has_error())
      return added;
  std::vector<PathSegmentReference> segments;
  for (std::size_t i = 0; i < 4; ++i) {
    const SketchEntityId line_id(tag + ".line." + std::to_string(i));
    if (auto added = sketch.add_line(
            SketchLine3D::create(line_id, SketchEntityId(tag + ".p" + std::to_string(i)),
                                 SketchEntityId(tag + ".p" + std::to_string((i + 1) % 4))).value());
        added.has_error())
      return added;
    segments.push_back(
        PathSegmentReference::create_sketch_3d(Sketch3DId("sketch3d." + tag), line_id,
                                               Sketch3DPathCurveKind::Line).value());
  }
  if (auto added = part.add_sketch_3d(std::move(sketch)); added.has_error()) return added;
  if (auto added = part.add_path_curve(
          PathCurve::create(PathCurveId("path." + tag), tag, segments, PathClosure::Closed,
                            PathOrientationRule::MinimumTwist).value());
      added.has_error())
    return added;
  return part.add_path_curve(PathCurve::create(PathCurveId("edge." + tag), tag + " edge",
                                               {segments.front()}, PathClosure::Open,
                                               PathOrientationRule::MinimumTwist).value());
}

Result<std::size_t> add_body(PartDocument& part, const char* id, BodyKind kind) {
  return part.add_body(Body::create(BodyId(id), id, kind).value());
}

Result<std::size_t> add_fill(PartDocument& part, const char* feature, const char* path,
                             const char* body) {
  return part.add_surface_feature(
      FillSurfaceFeature::create(FeatureId(feature), feature,
                                 {BoundaryCurveReference::create_path_curve(PathCurveId(path)).value()},
                                 BodyId(body)).value());
}

// A Part with two coplanar quads pre-filled into body.left/body.right (offset by
// `gap` in x), a standalone quad for interactive fill, plus spare result bodies.
void build_surface_part(GuiDocumentSession& session, double gap = 0.0) {
  REQUIRE(session.create_part(DocumentId("part.surface"), "Surface"));
  REQUIRE(session.commit_part_transaction("Setup", [gap](PartDocument& part) -> Result<std::size_t> {
    for (auto parameter : {length("stitch.tolerance", 0.001), length("extend.distance", 2.0)})
      if (auto added = part.add_parameter(std::move(parameter)); added.has_error()) return added;
    if (auto added = add_quad(part, "left",
                              {{{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}}});
        added.has_error())
      return added;
    if (auto added = add_quad(part, "right",
                              {{{10 + gap, 0, 0}, {20 + gap, 0, 0}, {20 + gap, 10, 0},
                                {10 + gap, 10, 0}}});
        added.has_error())
      return added;
    if (auto added = add_quad(part, "solo",
                              {{{0, 20, 0}, {10, 20, 0}, {10, 30, 0}, {0, 30, 0}}});
        added.has_error())
      return added;
    for (const char* body : {"body.left", "body.right"})
      if (auto added = add_body(part, body, BodyKind::Surface); added.has_error()) return added;
    for (const char* body : {"body.shell", "body.solo", "body.extend"})
      if (auto added = add_body(part, body, BodyKind::Surface); added.has_error()) return added;
    if (auto added = add_body(part, "body.solid", BodyKind::Solid); added.has_error()) return added;
    if (auto added = add_fill(part, "feature.fill_left", "path.left", "body.left");
        added.has_error())
      return added;
    return add_fill(part, "feature.fill_right", "path.right", "body.right");
  }));
}

SurfaceReference body_surface(const char* body) {
  return SurfaceReference::create_body(BodyId(body)).value();
}
BoundaryCurveReference boundary(const char* path) {
  return BoundaryCurveReference::create_path_curve(PathCurveId(path)).value();
}

GuiViewportManipulatorMapping planar_mapping() {
  GuiViewportCameraBookmark camera;
  camera.eye = {0.0, 0.0, 100.0};
  camera.target = {0.0, 0.0, 0.0};
  camera.up_direction = {0.0, 1.0, 0.0};
  camera.scale = 200.0;
  camera.projection = GuiViewportProjection::Orthographic;
  return GuiViewportManipulatorMapping::create_camera(camera, 200.0, 200.0).value();
}
GuiFeatureManipulatorFrame planar_frame() {
  GuiFeatureManipulatorFrame frame;
  frame.axis = {1.0, 0.0, 0.0};
  return frame;
}

} // namespace

TEST_CASE("Block 128 interactive Fill and Extend author surfaces and commit one transaction",
          "[gui][interactive-surface]") {
  GuiDocumentSession session;
  build_surface_part(session);

  SECTION("fill collects a boundary curve and applies a surface body") {
    GuiInteractiveSurfaceController fill;
    REQUIRE(fill.begin_fill(session, FeatureId("feature.solo"), "Solo", BodyId("body.solo")));
    CHECK(fill.kind() == SurfaceFeatureKind::FillSurface);
    CHECK(fill.add_boundary_curve(boundary("path.solo")));
    CHECK(fill.add_boundary_curve(boundary("path.solo")).has_error());  // duplicate
    CHECK(fill.boundary_count() == 1);
    REQUIRE(fill.preview(session));
    CHECK(session.part_document()->surface_features().size() == 2);  // only the two setup fills
    REQUIRE(fill.apply(session));
    CHECK(session.part_document()->surface_features().size() == 3);
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.solo")) != nullptr);
  }

  SECTION("extend drags a boundary distance and applies") {
    GuiInteractiveSurfaceController extend;
    REQUIRE(extend.begin_extend(session, FeatureId("feature.extend"), "Extend",
                                body_surface("body.left"), boundary("edge.left"),
                                ParameterId("extend.distance"), BodyId("body.extend")));
    CHECK(extend.distance() == 2.0);
    REQUIRE(extend.handles().size() == 1);
    CHECK(extend.handles().front().id == std::string(kSurfaceExtendHandleId));
    extend.set_distance(4.0);
    REQUIRE(extend.preview(session));
    REQUIRE(extend.apply(session));
    CHECK(session.part_document()->find_parameter(ParameterId("extend.distance"))
              ->value().millimeters() == 4.0);
    CHECK(session.part_document()->surface_features().size() == 3);
  }

  SECTION("closed-shell-to-solid reports an open shell before commit") {
    GuiInteractiveSurfaceController convert;
    REQUIRE(convert.begin_closed_shell_to_solid(session, FeatureId("feature.solid"), "Solid",
                                                body_surface("body.left"), BodyId("body.solid")));
    const auto preview = convert.preview(session);
    REQUIRE(preview.has_error());  // a single open quad is not a closed shell
    CHECK(session.part_document()->surface_features().size() == 2);
  }

  SECTION("surface commands fail closed on missing bodies, parameters, and inputs") {
    GuiInteractiveSurfaceController surface;
    CHECK(surface.begin_fill(session, FeatureId("f"), "F", BodyId("body.absent")).has_error());
    CHECK(surface.begin_stitch(session, FeatureId("f"), "F", ParameterId("absent"),
                               BodyId("body.shell")).has_error());
    REQUIRE(surface.begin_stitch(session, FeatureId("f"), "F", ParameterId("stitch.tolerance"),
                                 BodyId("body.shell")));
    REQUIRE(surface.add_surface(body_surface("body.left")));
    CHECK(surface.preview(session).has_error());  // stitch needs at least two surfaces
  }
}

TEST_CASE("Block 128 stitch surfaces the free-edge/gap diagnostic driven by the typed tolerance",
          "[integration][stitch-diagnostic-overlay]") {
  SECTION("coplanar surfaces stitch cleanly into one shell") {
    GuiDocumentSession session;
    build_surface_part(session, 0.0);
    GuiInteractiveSurfaceController stitch;
    REQUIRE(stitch.begin_stitch(session, FeatureId("feature.stitch"), "Stitch",
                                ParameterId("stitch.tolerance"), BodyId("body.shell")));
    REQUIRE(stitch.add_surface(body_surface("body.left")));
    REQUIRE(stitch.add_surface(body_surface("body.right")));
    CHECK(stitch.add_surface(body_surface("body.left")).has_error());  // duplicate
    CHECK(stitch.surface_count() == 2);
    REQUIRE(stitch.preview(session));
    REQUIRE(stitch.apply(session));
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.shell")) != nullptr);
  }

  SECTION("a gap beyond the tolerance surfaces a diagnostic; a looser tolerance resolves it") {
    GuiDocumentSession session;
    build_surface_part(session, 0.1);  // 0.1 mm gap between the two quads
    GuiInteractiveSurfaceController stitch;
    REQUIRE(stitch.begin_stitch(session, FeatureId("feature.stitch"), "Stitch",
                                ParameterId("stitch.tolerance"), BodyId("body.shell")));
    REQUIRE(stitch.add_surface(body_surface("body.left")));
    REQUIRE(stitch.add_surface(body_surface("body.right")));

    stitch.set_tolerance(0.001);  // tighter than the gap
    const auto tight = stitch.preview(session);
    REQUIRE(tight.has_error());
    const std::string message = tight.error().message();
    CHECK((message.find("free") != std::string::npos || message.find("gap") != std::string::npos ||
           message.find("connect") != std::string::npos));
    CHECK(session.part_document()->surface_features().size() == 2);

    stitch.set_tolerance(0.5);  // looser than the gap
    REQUIRE(stitch.preview(session));
    REQUIRE(stitch.apply(session));
    CHECK(session.part_document()->surface_features().size() == 3);
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.shell")) != nullptr);
  }

  SECTION("the coordinator routes an extend-distance drag identically to a typed distance") {
    GuiDocumentSession dragged;
    build_surface_part(dragged);
    GuiViewportManipulatorLayer layer;
    GuiInteractiveFeatureCoordinator coordinator(&dragged, &layer);
    REQUIRE(layer.set_mapping(planar_mapping()));
    coordinator.cancel();
    REQUIRE(coordinator.surface().begin_extend(dragged, FeatureId("feature.extend"), "Extend",
                                               body_surface("body.left"), boundary("edge.left"),
                                               ParameterId("extend.distance"), BodyId("body.extend")));
    coordinator.surface().set_manipulator_frame(planar_frame());
    REQUIRE(layer.set_handles(coordinator.surface().handles()));
    REQUIRE(layer.begin_drag({100.0, 100.0}));
    const auto candidate = layer.end_drag({103.0, 100.0});  // +3 mm -> distance 5
    REQUIRE(candidate.has_value());
    coordinator.on_candidate(*candidate);
    const double distance = coordinator.surface().distance();
    CHECK(std::abs(distance - 5.0) < 1.0e-9);
    REQUIRE(coordinator.apply());

    GuiDocumentSession typed;
    build_surface_part(typed);
    GuiInteractiveSurfaceController headless;
    REQUIRE(headless.begin_extend(typed, FeatureId("feature.extend"), "Extend",
                                  body_surface("body.left"), boundary("edge.left"),
                                  ParameterId("extend.distance"), BodyId("body.extend")));
    headless.set_distance(distance);
    REQUIRE(headless.apply(typed));

    CHECK(serialize_part_document_to_json(*dragged.part_document()).value() ==
          serialize_part_document_to_json(*typed.part_document()).value());
  }
}
