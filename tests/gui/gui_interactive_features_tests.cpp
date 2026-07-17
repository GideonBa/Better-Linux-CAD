#include "blcad/gui/gui_interactive_extrude_revolve.hpp"
#include "blcad/gui/main_window.hpp"

#include "blcad/core/body.hpp"
#include "blcad/core/construction_geometry.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>

#include <cmath>
#include <string>

using namespace blcad;
using namespace blcad::gui;

namespace {

Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}

Result<std::size_t> add_body(PartDocument& part, const char* id) {
  return part.add_body(Body::create(BodyId(id), id, BodyKind::Solid).value());
}

// A Part with a base profile plus a smaller centred profile (for through-all
// cuts), a datum, and three solid Bodies so distinct NewBody features never
// collide.
void build_extrude_part(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("part.extrude"), "Extrude"));
  REQUIRE(session.commit_part_transaction("Setup", [](PartDocument& part) -> Result<std::size_t> {
    for (auto parameter : {length("width", 40.0), length("height", 25.0), length("depth", 12.0),
                           length("wall", 2.0), length("hole.w", 10.0), length("hole.h", 10.0)})
      if (auto added = part.add_parameter(std::move(parameter)); added.has_error()) return added;
    if (auto added = part.add_datum_plane(DatumPlane::xy().value()); added.has_error()) return added;
    auto base = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
    if (auto added = base.add_profile(RectangleProfile::create(ProfileId("profile.base"),
                                                               ParameterId("width"),
                                                               ParameterId("height")).value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(base)); added.has_error()) return added;
    auto hole = Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy")).value();
    if (auto added = hole.add_profile(RectangleProfile::create(ProfileId("profile.hole"),
                                                               ParameterId("hole.w"),
                                                               ParameterId("hole.h")).value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(hole)); added.has_error()) return added;
    for (const char* body : {"body.main", "body.two", "body.three"})
      if (auto added = add_body(part, body); added.has_error()) return added;
    return Result<std::size_t>::success(1);
  }));
}

// A Part with a rectangular profile offset from the Y axis plus a construction
// axis and two solid Bodies, ready for interactive Revolves.
void build_revolve_part(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("part.revolve"), "Revolve"));
  REQUIRE(session.commit_part_transaction("Setup", [](PartDocument& part) -> Result<std::size_t> {
    for (auto parameter : {length("rev.width", 10.0), length("rev.height", 20.0)})
      if (auto added = part.add_parameter(std::move(parameter)); added.has_error()) return added;
    if (auto added = part.add_datum_plane(DatumPlane::xy().value()); added.has_error()) return added;
    auto sketch = Sketch::create(SketchId("sketch.rev"), "Rev", DatumPlaneId("datum.xy")).value();
    if (auto added = sketch.add_profile(RectangleProfile::create(ProfileId("profile.rev"),
                                                                 ParameterId("rev.width"),
                                                                 ParameterId("rev.height"),
                                                                 {15.0, 0.0}).value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(sketch)); added.has_error()) return added;
    if (auto added = part.add_construction_line(
            ConstructionLine::create_explicit(ConstructionLineId("axis.y"), "Y", {},
                                              {0.0, 1.0, 0.0}).value());
        added.has_error())
      return added;
    for (const char* body : {"body.rev", "body.rev2"})
      if (auto added = add_body(part, body); added.has_error()) return added;
    return Result<std::size_t>::success(1);
  }));
}

AxisReference revolve_axis() {
  return AxisReference::create_construction_line(PartFeatureInputRole::RevolveAxis,
                                                 ConstructionLineId("axis.y"),
                                                 PartFeatureInputCapability::Axis)
      .value();
}

// Deterministic orthographic camera mapping (reused from the Block-123 proof):
// model_per_dip == 1, screen centre (100,100), model X maps to screen X.
GuiViewportManipulatorMapping planar_mapping() {
  GuiViewportCameraBookmark camera;
  camera.eye = {0.0, 0.0, 100.0};
  camera.target = {0.0, 0.0, 0.0};
  camera.up_direction = {0.0, 1.0, 0.0};
  camera.scale = 200.0;
  camera.projection = GuiViewportProjection::Orthographic;
  return GuiViewportManipulatorMapping::create_camera(camera, 200.0, 200.0).value();
}

// A screen-parallel extent frame so the linear drag maps model X to screen X.
GuiFeatureManipulatorFrame planar_frame() {
  GuiFeatureManipulatorFrame frame;
  frame.origin = {0.0, 0.0, 0.0};
  frame.axis = {1.0, 0.0, 0.0};
  frame.plane_normal = {0.0, 0.0, 1.0};
  frame.reference_direction = {1.0, 0.0, 0.0};
  return frame;
}

} // namespace

TEST_CASE("Block 124 interactive Extrude drives a distance parameter and commits one transaction",
          "[gui][interactive-extrude]") {
  GuiDocumentSession session;
  build_extrude_part(session);

  GuiInteractiveExtrudeController extrude;
  REQUIRE(extrude.begin(session, SketchId("sketch.base"), ProfileId("profile.base"),
                        FeatureId("feature.extrude"), "Extrude", ParameterId("depth"),
                        BodyId("body.main")));
  CHECK(extrude.active());
  CHECK(extrude.distance() == 12.0);
  CHECK(extrude.extent_handle().has_value());
  CHECK(extrude.extent_handle()->reference_value == 12.0);

  extrude.set_distance(20.0);
  const auto preview = extrude.preview(session);
  REQUIRE(preview);
  CHECK(preview.value().semantic_summary.find("additive extrude") != std::string::npos);
  CHECK(session.part_document()->feature_count() == 0);

  REQUIRE(extrude.apply(session));
  CHECK_FALSE(extrude.active());
  CHECK(session.part_document()->find_feature(FeatureId("feature.extrude")) != nullptr);
  CHECK(session.part_document()->find_parameter(ParameterId("depth"))->value().millimeters() == 20.0);
  REQUIRE(session.recompute());
  const auto* cache = session.part_shape_cache();
  REQUIRE(cache != nullptr);
  CHECK(cache->find_body_result(BodyId("body.main")) != nullptr);

  REQUIRE(session.undo());
  CHECK(session.part_document()->find_feature(FeatureId("feature.extrude")) == nullptr);
  CHECK(session.part_document()->find_parameter(ParameterId("depth"))->value().millimeters() == 12.0);
}

TEST_CASE("Block 124 interactive Extrude exposes operation, taper, thin, and through-all modes",
          "[gui][interactive-extrude]") {
  GuiDocumentSession session;
  build_extrude_part(session);

  SECTION("taper and thin expose their own handles and preview") {
    GuiInteractiveExtrudeController thinned;
    REQUIRE(thinned.begin(session, SketchId("sketch.base"), ProfileId("profile.base"),
                          FeatureId("feature.thin"), "Thin", ParameterId("depth"),
                          BodyId("body.main")));
    thinned.set_taper(5.0);
    thinned.set_thin(ExtrudeThinMode::OneSided, ParameterId("wall"), 2.0);
    CHECK(thinned.taper_handle().has_value());
    CHECK(thinned.taper_handle()->reference_value == 5.0);
    CHECK(thinned.thin_handle().has_value());
    CHECK(thinned.thin_handle()->minimum_value == 0.0);
    REQUIRE(thinned.preview(session));
  }

  SECTION("through-all cut targets an existing solid without an extent handle") {
    GuiInteractiveExtrudeController base;
    REQUIRE(base.begin(session, SketchId("sketch.base"), ProfileId("profile.base"),
                       FeatureId("feature.base"), "Base", ParameterId("depth"),
                       BodyId("body.main")));
    REQUIRE(base.apply(session));
    REQUIRE(session.recompute());

    GuiInteractiveExtrudeController cut;
    REQUIRE(cut.begin(session, SketchId("sketch.hole"), ProfileId("profile.hole"),
                      FeatureId("feature.cut"), "Cut", ParameterId("depth"), BodyId("body.main")));
    cut.set_operation(FeatureBodyOperationMode::Cut, BodyId("body.main"));
    cut.set_extent_mode(ExtrudeExtentMode::ThroughAll);
    CHECK_FALSE(cut.extent_handle().has_value());
    REQUIRE(cut.preview(session));
    REQUIRE(cut.apply(session));
    CHECK(session.part_document()->find_feature(FeatureId("feature.cut")) != nullptr);
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.main")) != nullptr);
  }
}

TEST_CASE("Block 124 interactive Extrude fails closed on invalid inputs",
          "[gui][interactive-extrude]") {
  GuiDocumentSession session;
  build_extrude_part(session);
  GuiInteractiveExtrudeController extrude;

  CHECK(extrude.begin(session, SketchId("sketch.base"), ProfileId("profile.missing"),
                      FeatureId("f"), "F", ParameterId("depth"), BodyId("body.main")).has_error());
  CHECK(extrude.begin(session, SketchId("sketch.base"), ProfileId("profile.base"), FeatureId("f"),
                      "F", ParameterId("nope"), BodyId("body.main")).has_error());

  REQUIRE(extrude.begin(session, SketchId("sketch.base"), ProfileId("profile.base"),
                        FeatureId("feature.two"), "Two", ParameterId("depth"), BodyId("body.main")));
  extrude.set_extent_mode(ExtrudeExtentMode::TwoSided);
  CHECK(extrude.preview(session).has_error());
}

TEST_CASE("Block 124 interactive Revolve drives full, angle, and symmetric extents",
          "[gui][interactive-revolve]") {
  GuiDocumentSession session;
  build_revolve_part(session);

  GuiInteractiveRevolveController revolve;
  REQUIRE(revolve.begin(session, SketchId("sketch.rev"), ProfileId("profile.rev"),
                        FeatureId("feature.revolve"), "Revolve", revolve_axis(),
                        BodyId("body.rev")));
  CHECK(revolve.active());
  CHECK(revolve.extent_mode() == RevolveExtentMode::Full);
  CHECK_FALSE(revolve.angle_handle().has_value());

  REQUIRE(revolve.preview(session));
  REQUIRE(revolve.apply(session));
  REQUIRE(session.recompute());
  CHECK(session.part_shape_cache()->find_body_result(BodyId("body.rev")) != nullptr);

  GuiInteractiveRevolveController partial;
  REQUIRE(partial.begin(session, SketchId("sketch.rev"), ProfileId("profile.rev"),
                        FeatureId("feature.partial"), "Partial", revolve_axis(),
                        BodyId("body.rev2")));
  partial.set_angle(90.0);
  CHECK(partial.angle_handle().has_value());
  CHECK(partial.angle_handle()->reference_value == 90.0);
  REQUIRE(partial.preview(session));

  partial.set_symmetric(120.0);
  CHECK(partial.extent_mode() == RevolveExtentMode::Symmetric);
  REQUIRE(partial.preview(session));

  partial.set_angle(0.0);
  CHECK(partial.preview(session).has_error());
}

TEST_CASE("Block 124 manipulator candidates author extrude and revolve identically to headless edits",
          "[integration][extrude-revolve-manipulator]") {
  SECTION("extent-arrow drag folds into the same committed document as a typed distance") {
    GuiDocumentSession dragged;
    build_extrude_part(dragged);
    GuiInteractiveExtrudeController controller;
    REQUIRE(controller.begin(dragged, SketchId("sketch.base"), ProfileId("profile.base"),
                             FeatureId("feature.extrude"), "Extrude", ParameterId("depth"),
                             BodyId("body.main")));
    controller.set_manipulator_frame(planar_frame());

    GuiViewportManipulatorLayer layer;
    REQUIRE(layer.set_mapping(planar_mapping()));
    REQUIRE(layer.set_handles({*controller.extent_handle()}));
    REQUIRE(layer.begin_drag({100.0, 100.0}));
    const auto candidate = layer.end_drag({130.0, 100.0});
    REQUIRE(candidate.has_value());
    CHECK(std::abs(candidate->scalar_value - 42.0) < 1.0e-9);
    CHECK(controller.apply_manipulator(*candidate));
    const double distance = controller.distance();
    REQUIRE(controller.apply(dragged));

    GuiDocumentSession typed;
    build_extrude_part(typed);
    GuiInteractiveExtrudeController headless;
    REQUIRE(headless.begin(typed, SketchId("sketch.base"), ProfileId("profile.base"),
                           FeatureId("feature.extrude"), "Extrude", ParameterId("depth"),
                           BodyId("body.main")));
    headless.set_distance(distance);
    REQUIRE(headless.apply(typed));

    CHECK(serialize_part_document_to_json(*dragged.part_document()).value() ==
          serialize_part_document_to_json(*typed.part_document()).value());
  }

  SECTION("angle-wheel drag folds into the same committed document as a typed angle") {
    GuiDocumentSession dragged;
    build_revolve_part(dragged);
    GuiInteractiveRevolveController controller;
    REQUIRE(controller.begin(dragged, SketchId("sketch.rev"), ProfileId("profile.rev"),
                             FeatureId("feature.revolve"), "Revolve", revolve_axis(),
                             BodyId("body.rev")));
    controller.set_angle(45.0);

    GuiViewportManipulatorLayer layer;
    REQUIRE(layer.set_mapping(planar_mapping()));
    REQUIRE(layer.set_handles({*controller.angle_handle()}));
    // Default 68-DIP ring: drag from 0 degrees (168,100) to +90 degrees (100,32),
    // adding 90 to the 45-degree reference.
    REQUIRE(layer.begin_drag({168.0, 100.0}));
    const auto candidate = layer.end_drag({100.0, 32.0});
    REQUIRE(candidate.has_value());
    CHECK(std::abs(candidate->scalar_value - 135.0) < 1.0e-9);
    CHECK(controller.apply_manipulator(*candidate));
    const double angle = controller.angle();
    REQUIRE(controller.apply(dragged));

    GuiDocumentSession typed;
    build_revolve_part(typed);
    GuiInteractiveRevolveController headless;
    REQUIRE(headless.begin(typed, SketchId("sketch.rev"), ProfileId("profile.rev"),
                           FeatureId("feature.revolve"), "Revolve", revolve_axis(),
                           BodyId("body.rev")));
    headless.set_angle(angle);
    REQUIRE(headless.apply(typed));

    CHECK(serialize_part_document_to_json(*dragged.part_document()).value() ==
          serialize_part_document_to_json(*typed.part_document()).value());
  }
}

TEST_CASE("Block 124 MainWindow owns the interactive feature coordinator and commits a drag",
          "[gui][interactive-extrude][gui][modeling-workspace]") {
  REQUIRE(qApp != nullptr);
  MainWindow window;
  build_extrude_part(window.session());

  auto& coordinator = window.interactive_features();
  REQUIRE(coordinator.begin_extrude(SketchId("sketch.base"), ProfileId("profile.base"),
                                    FeatureId("feature.extrude"), "Extrude", ParameterId("depth"),
                                    BodyId("body.main"), planar_frame()));
  CHECK(coordinator.active());

  auto& layer = window.viewport_manipulators();
  REQUIRE(layer.set_mapping(planar_mapping()));
  REQUIRE(layer.handles().size() == 1);

  REQUIRE(layer.begin_drag({100.0, 100.0}));
  const auto candidate = layer.end_drag({130.0, 100.0});
  REQUIRE(candidate.has_value());
  coordinator.on_candidate(*candidate);
  CHECK(std::abs(coordinator.extrude().distance() - 42.0) < 1.0e-9);

  REQUIRE(coordinator.preview());
  REQUIRE(coordinator.apply());
  CHECK_FALSE(coordinator.active());
  CHECK(window.session().part_document()->find_feature(FeatureId("feature.extrude")) != nullptr);
  CHECK(window.viewport_manipulators().handles().empty());
}
