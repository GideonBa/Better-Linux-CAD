#include "blcad/gui/gui_interactive_extrude_revolve_binder.hpp"
#include "blcad/gui/gui_interactive_pattern_body.hpp"

#include "blcad/core/body.hpp"
#include "blcad/core/construction_geometry.hpp"
#include "blcad/core/datum_plane.hpp"
#include "blcad/core/feature.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

using namespace blcad;
using namespace blcad::gui;

namespace {

Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}
Parameter count(const char* id, double value) {
  return Parameter::create_count(ParameterId(id), id, Quantity::count(value, id).value()).value();
}

Result<std::size_t> add_solid(PartDocument& part, const char* id) {
  return part.add_body(Body::create(BodyId(id), id, BodyKind::Solid).value());
}

Feature box(const char* feature_id, const char* sketch_id, const char* body_id) {
  return Feature::create_additive_extrude(FeatureId(feature_id), feature_id, SketchId(sketch_id),
                                          ParameterId("pd"))
      .value()
      .with_body_result_context(
          FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                           BodyId(body_id)).value())
      .value();
}

// Two overlapping boxes offset from the origin (body.main at x=30, body.tool at
// x=32), a linear-direction datum axis, a circular construction axis, plus spare
// result Bodies for pattern/mirror/circular/boolean output.
void build_pattern_part(GuiDocumentSession& session) {
  REQUIRE(session.create_part(DocumentId("part.pattern"), "Pattern"));
  REQUIRE(session.commit_part_transaction("Setup", [](PartDocument& part) -> Result<std::size_t> {
    for (auto parameter : {length("pw", 10.0), length("ph", 10.0), length("pd", 5.0),
                           length("spacing", 25.0)})
      if (auto added = part.add_parameter(std::move(parameter)); added.has_error()) return added;
    if (auto added = part.add_parameter(count("instances", 2.0)); added.has_error()) return added;
    if (auto added = part.add_datum_plane(DatumPlane::xy().value()); added.has_error()) return added;
    if (auto added = part.add_datum_axis(
            DatumAxis::create_explicit(DatumAxisId("axis.x"), "X", {}, {1.0, 0.0, 0.0}).value());
        added.has_error())
      return added;
    if (auto added = part.add_construction_line(
            ConstructionLine::create_explicit(ConstructionLineId("axis.z"), "Z", {},
                                              {0.0, 0.0, 1.0}).value());
        added.has_error())
      return added;
    auto base = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
    if (auto added = base.add_profile(RectangleProfile::create(ProfileId("profile.base"),
                                                               ParameterId("pw"), ParameterId("ph"),
                                                               {30.0, 0.0}).value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(base)); added.has_error()) return added;
    auto tool = Sketch::create(SketchId("sketch.tool"), "Tool", DatumPlaneId("datum.xy")).value();
    if (auto added = tool.add_profile(RectangleProfile::create(ProfileId("profile.tool"),
                                                               ParameterId("pw"), ParameterId("ph"),
                                                               {32.0, 0.0}).value());
        added.has_error())
      return added;
    if (auto added = part.add_sketch(std::move(tool)); added.has_error()) return added;
    for (const char* body : {"body.main", "body.tool", "body.pattern", "body.mirror",
                             "body.circular", "body.bool"})
      if (auto added = add_solid(part, body); added.has_error()) return added;
    if (auto added = part.add_feature(box("feature.base", "sketch.base", "body.main"));
        added.has_error())
      return added;
    return part.add_feature(box("feature.tool", "sketch.tool", "body.tool"));
  }));
}

AxisReference linear_axis() {
  return AxisReference::create_datum_axis(PartFeatureInputRole::PatternAxis, DatumAxisId("axis.x"))
      .value();
}
AxisReference circular_axis() {
  return AxisReference::create_construction_line(PartFeatureInputRole::PatternAxis,
                                                 ConstructionLineId("axis.z"),
                                                 PartFeatureInputCapability::Axis)
      .value();
}
PlaneReference mirror_plane() {
  return PlaneReference::create_datum_plane(PartFeatureInputRole::MirrorPlane,
                                            DatumPlaneId("datum.xy"))
      .value();
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

TEST_CASE("Block 126 interactive Linear/Circular Pattern and Mirror collect sources and commit",
          "[gui][interactive-pattern-mirror]") {
  GuiDocumentSession session;
  build_pattern_part(session);

  SECTION("linear pattern drives count and spacing parameters with exact undo") {
    GuiInteractivePatternMirrorController pattern;
    REQUIRE(pattern.begin_linear_pattern(session, FeatureId("feature.linear"), "Linear",
                                         linear_axis(), ParameterId("instances"),
                                         LinearPatternExtentMode::Spacing, ParameterId("spacing"),
                                         BodyId("body.pattern")));
    CHECK(pattern.count() == 2);
    CHECK(pattern.spacing() == 25.0);
    REQUIRE(pattern.add_source_body(BodyId("body.main")));
    CHECK(pattern.add_source_body(BodyId("body.main")).has_error());  // duplicate
    CHECK(pattern.source_count() == 1);
    REQUIRE(pattern.handles().size() == 2);
    pattern.set_count(3);
    pattern.set_spacing(30.0);

    REQUIRE(pattern.preview(session));
    CHECK(session.part_document()->linear_pattern_features().empty());
    REQUIRE(pattern.apply(session));
    CHECK(session.part_document()->linear_pattern_features().size() == 1);
    CHECK(session.part_document()->find_parameter(ParameterId("instances"))->value().count_value() == 3);
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.pattern")) != nullptr);

    REQUIRE(session.undo());
    CHECK(session.part_document()->linear_pattern_features().empty());
    CHECK(session.part_document()->find_parameter(ParameterId("instances"))->value().count_value() == 2);
  }

  SECTION("circular pattern drives a literal total angle and count parameter") {
    GuiInteractivePatternMirrorController pattern;
    REQUIRE(pattern.begin_circular_pattern(session, FeatureId("feature.circular"), "Circular",
                                           circular_axis(), ParameterId("instances"),
                                           BodyId("body.circular")));
    REQUIRE(pattern.add_source_body(BodyId("body.main")));
    pattern.set_count(3);
    pattern.set_angle(360.0);
    REQUIRE(pattern.handles().size() == 2);
    CHECK(pattern.handles().front().id == std::string(kPatternAngleHandleId));
    REQUIRE(pattern.preview(session));
    REQUIRE(pattern.apply(session));
    CHECK(session.part_document()->circular_pattern_features().size() == 1);
  }

  SECTION("mirror reflects a source across a plane without a scalar handle") {
    GuiInteractivePatternMirrorController mirror;
    REQUIRE(mirror.begin_mirror(session, FeatureId("feature.mirror"), "Mirror", mirror_plane(),
                                BodyId("body.mirror")));
    CHECK(mirror.handles().empty());
    REQUIRE(mirror.add_source_body(BodyId("body.main")));
    REQUIRE(mirror.preview(session));
    REQUIRE(mirror.apply(session));
    CHECK(session.part_document()->mirror_features().size() == 1);
  }

  SECTION("pattern fails closed on missing parameters, result body, and empty sources") {
    GuiInteractivePatternMirrorController pattern;
    CHECK(pattern.begin_linear_pattern(session, FeatureId("f"), "F", linear_axis(),
                                       ParameterId("spacing"), LinearPatternExtentMode::Spacing,
                                       ParameterId("spacing"), BodyId("body.pattern"))
              .has_error());  // count param is Length, not Count
    CHECK(pattern.begin_linear_pattern(session, FeatureId("f"), "F", linear_axis(),
                                       ParameterId("instances"), LinearPatternExtentMode::Spacing,
                                       ParameterId("spacing"), BodyId("body.absent"))
              .has_error());  // result body missing
    REQUIRE(pattern.begin_linear_pattern(session, FeatureId("f"), "F", linear_axis(),
                                         ParameterId("instances"), LinearPatternExtentMode::Spacing,
                                         ParameterId("spacing"), BodyId("body.pattern")));
    CHECK(pattern.preview(session).has_error());  // no sources
  }
}

TEST_CASE("Block 126 interactive Body Boolean and Body Transform commit one record each",
          "[gui][interactive-body-operation]") {
  GuiDocumentSession session;
  build_pattern_part(session);

  SECTION("boolean color-codes roles, toggles keep, and applies a NewBody union") {
    GuiInteractiveBodyOperationController boolean;
    REQUIRE(boolean.begin_boolean(session, FeatureId("feature.bool"), BodyBooleanOperation::Add,
                                  BodyId("body.main"), BodyBooleanResultMode::NewBody,
                                  BodyId("body.bool")));
    CHECK(boolean.kind() == GuiBodyOperationKind::Boolean);
    CHECK(boolean.add_tool_body(session, BodyId("body.main")).has_error());  // target cannot be a tool
    REQUIRE(boolean.add_tool_body(session, BodyId("body.tool")));
    CHECK(boolean.add_tool_body(session, BodyId("body.tool")).has_error());  // duplicate
    CHECK(boolean.tool_count() == 1);
    boolean.set_keep_tool_bodies(true);
    CHECK(boolean.handles().empty());
    REQUIRE(boolean.preview(session));
    REQUIRE(boolean.apply(session));
    CHECK(session.part_document()->body_boolean_features().size() == 1);
    REQUIRE(session.recompute());
    CHECK(session.part_shape_cache()->find_body_result(BodyId("body.bool")) != nullptr);
  }

  SECTION("transform triad appends translate, rotate, and scale to the stack") {
    GuiInteractiveBodyOperationController transform;
    REQUIRE(transform.begin_translate(session, BodyTransformId("move.1"), BodyId("body.main")));
    CHECK(transform.transform_kind() == BodyTransformKind::Translate);
    CHECK(transform.handles().size() == 3);
    transform.set_translation({0.0, 20.0, 0.0});
    REQUIRE(transform.preview(session));
    REQUIRE(transform.apply(session));
    CHECK(session.part_document()->body_transforms().size() == 1);

    GuiInteractiveBodyOperationController rotate;
    REQUIRE(rotate.begin_rotate(session, BodyTransformId("rot.1"), BodyId("body.main"),
                                BodyTransformRotationAxis::create_datum_axis(DatumAxisId("axis.x"))
                                    .value()));
    CHECK(rotate.handles().size() == 1);
    CHECK(rotate.handles().front().id == std::string(kTransformAngleHandleId));
    rotate.set_angle(30.0);
    REQUIRE(rotate.apply(session));
    CHECK(session.part_document()->body_transforms().size() == 2);

    GuiInteractiveBodyOperationController scale;
    REQUIRE(scale.begin_uniform_scale(session, BodyTransformId("scale.1"), BodyId("body.main")));
    CHECK(scale.handles().front().id == std::string(kTransformScaleHandleId));
    scale.set_scale_factor(2.0);
    REQUIRE(scale.apply(session));
    CHECK(session.part_document()->body_transforms().size() == 3);
  }

  SECTION("boolean and transform fail closed on missing bodies") {
    GuiInteractiveBodyOperationController boolean;
    CHECK(boolean.begin_boolean(session, FeatureId("f"), BodyBooleanOperation::Add,
                                BodyId("body.absent"), BodyBooleanResultMode::ModifyTarget,
                                std::nullopt).has_error());
    CHECK(boolean.begin_boolean(session, FeatureId("f"), BodyBooleanOperation::Add,
                                BodyId("body.main"), BodyBooleanResultMode::NewBody,
                                std::nullopt).has_error());  // NewBody needs a produced body
    GuiInteractiveBodyOperationController transform;
    CHECK(transform.begin_translate(session, BodyTransformId("t"), BodyId("body.absent")).has_error());
  }
}

TEST_CASE("Block 126 manipulator ghosts author patterns and transforms like headless edits",
          "[integration][pattern-ghost-preview]") {
  SECTION("linear pattern spacing drag folds into the same document as a typed spacing") {
    GuiDocumentSession dragged;
    build_pattern_part(dragged);
    GuiInteractivePatternMirrorController controller;
    REQUIRE(controller.begin_linear_pattern(dragged, FeatureId("feature.linear"), "Linear",
                                            linear_axis(), ParameterId("instances"),
                                            LinearPatternExtentMode::Spacing, ParameterId("spacing"),
                                            BodyId("body.pattern")));
    controller.set_manipulator_frame(planar_frame());
    controller.set_count(3);
    REQUIRE(controller.add_source_body(BodyId("body.main")));

    GuiViewportManipulatorLayer layer;
    REQUIRE(layer.set_mapping(planar_mapping()));
    REQUIRE(layer.set_handles({controller.handles().front()}));  // spacing handle only
    REQUIRE(layer.begin_drag({100.0, 100.0}));
    const auto candidate = layer.end_drag({125.0, 100.0});  // +25 mm -> spacing 50
    REQUIRE(candidate.has_value());
    CHECK(controller.apply_manipulator(*candidate));
    const double spacing = controller.spacing();
    CHECK(std::abs(spacing - 50.0) < 1.0e-9);
    REQUIRE(controller.apply(dragged));

    GuiDocumentSession typed;
    build_pattern_part(typed);
    GuiInteractivePatternMirrorController headless;
    REQUIRE(headless.begin_linear_pattern(typed, FeatureId("feature.linear"), "Linear",
                                          linear_axis(), ParameterId("instances"),
                                          LinearPatternExtentMode::Spacing, ParameterId("spacing"),
                                          BodyId("body.pattern")));
    headless.set_count(3);
    REQUIRE(headless.add_source_body(BodyId("body.main")));
    headless.set_spacing(spacing);
    REQUIRE(headless.apply(typed));

    CHECK(serialize_part_document_to_json(*dragged.part_document()).value() ==
          serialize_part_document_to_json(*typed.part_document()).value());
  }

  SECTION("transform triad drag through the coordinator matches a typed translation") {
    GuiDocumentSession dragged;
    build_pattern_part(dragged);
    GuiViewportManipulatorLayer layer;
    GuiInteractiveFeatureCoordinator coordinator(&dragged, &layer);
    REQUIRE(layer.set_mapping(planar_mapping()));
    coordinator.cancel();
    REQUIRE(coordinator.body_operation().begin_translate(dragged, BodyTransformId("move.1"),
                                                         BodyId("body.main")));
    coordinator.body_operation().set_manipulator_frame(planar_frame());
    // Drive only the X translate-triad handle to avoid triad hit ambiguity.
    REQUIRE(layer.set_handles({coordinator.body_operation().handles().front()}));
    REQUIRE(layer.begin_drag({100.0, 100.0}));
    const auto candidate = layer.end_drag({120.0, 100.0});  // +20 mm along X
    REQUIRE(candidate.has_value());
    coordinator.on_candidate(*candidate);
    const double x = coordinator.body_operation().translation().x;
    CHECK(std::abs(x - 20.0) < 1.0e-9);
    REQUIRE(coordinator.apply());

    GuiDocumentSession typed;
    build_pattern_part(typed);
    GuiInteractiveBodyOperationController headless;
    REQUIRE(headless.begin_translate(typed, BodyTransformId("move.1"), BodyId("body.main")));
    headless.set_translation({x, 0.0, 0.0});
    REQUIRE(headless.apply(typed));

    CHECK(serialize_part_document_to_json(*dragged.part_document()).value() ==
          serialize_part_document_to_json(*typed.part_document()).value());
  }
}
