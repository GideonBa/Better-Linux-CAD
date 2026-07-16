#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_spline_edit.hpp"
#include "blcad/core/sketch_text.hpp"
#include "blcad/core/sketch_text_json.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using namespace blcad;

namespace {

Sketch make_spline_profile_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.spline"), "SplineSketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  auto bottom =
      LineSegment::create(SketchEntityId("line.bottom"), Point2{0.0, 0.0}, Point2{20.0, 0.0});
  REQUIRE(bottom);
  REQUIRE(sketch.value().add_entity(bottom.value()));

  auto spline =
      SplineSegment::create_cubic_bezier(SketchEntityId("spline.right"), Point2{20.0, 0.0},
                                         Point2{30.0, 5.0}, Point2{30.0, 15.0}, Point2{20.0, 20.0});
  REQUIRE(spline);
  REQUIRE(sketch.value().add_entity(spline.value()));

  auto left =
      LineSegment::create(SketchEntityId("line.left"), Point2{20.0, 20.0}, Point2{0.0, 0.0});
  REQUIRE(left);
  REQUIRE(sketch.value().add_entity(left.value()));

  auto tangent = SketchTangentContinuity::create_tangent(
      SketchConstraintId("tangent.bottom_spline"), SketchEntityId("line.bottom"),
      SketchEntityId("spline.right"));
  REQUIRE(tangent);
  REQUIRE(sketch.value().add_tangent_continuity(tangent.value()));

  auto profile = ArcClosedProfile::create(
      ProfileId("profile.spline"),
      {SketchEntityId("line.bottom"), SketchEntityId("spline.right"), SketchEntityId("line.left")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));

  return sketch.value();
}

PartDocument make_document() {
  auto document = PartDocument::create(DocumentId("part.spline_json"), "SplineJson");
  REQUIRE(document);
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  REQUIRE(document.value().add_sketch(make_spline_profile_sketch()));
  return document.value();
}

Parameter length_parameter(const char* id, const char* name, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

} // namespace

TEST_CASE("Spline sketch profile and tangent metadata roundtrip through JSON",
          "[core][json][spline]") {
  PartDocument document = make_document();

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("\"spline_segments\"") != std::string::npos);
  CHECK(serialized.value().find("\"tangent_continuities\"") != std::string::npos);
  CHECK(serialized.value().find("\"kind\": \"cubic_bezier\"") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);

  const Sketch* sketch = restored.value().find_sketch(SketchId("sketch.spline"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->find_spline_segment(SketchEntityId("spline.right")) != nullptr);
  REQUIRE(sketch->find_tangent_continuity(SketchConstraintId("tangent.bottom_spline")) != nullptr);
  REQUIRE(sketch->find_arc_closed_profile(ProfileId("profile.spline")) != nullptr);

  const SplineSegment* spline = sketch->find_spline_segment(SketchEntityId("spline.right"));
  REQUIRE(spline != nullptr);
  CHECK(spline->control1() == Point2{30.0, 5.0});
  CHECK(spline->control2() == Point2{30.0, 15.0});
}

TEST_CASE("Block 113 control-point editing preserves ids profiles and JSON authority",
          "[core][sketch-spline-edit]") {
  PartDocument document = make_document();
  const Sketch* source = document.find_sketch(SketchId("sketch.spline"));
  REQUIRE(source != nullptr);

  auto model = SketchSplineEditModel::from_segments(
      *source, {SketchEntityId("spline.right")});
  REQUIRE(model);
  REQUIRE(model.value().move_control_point(0U, 1U, Point2{34.0, 6.0}));
  const auto preview = model.value().preview();
  REQUIRE(preview.control_polygons.size() == 1U);
  CHECK(preview.control_polygons.front()[1] == Point2{34.0, 6.0});

  auto candidate = model.value().build_sketch(*source);
  REQUIRE(candidate);
  REQUIRE(candidate.value().find_arc_closed_profile(ProfileId("profile.spline")) != nullptr);
  REQUIRE(document.update_sketch(std::move(candidate.value())));

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const auto* spline = restored.value()
                           .find_sketch(SketchId("sketch.spline"))
                           ->find_spline_segment(SketchEntityId("spline.right"));
  REQUIRE(spline != nullptr);
  CHECK(spline->control1() == Point2{34.0, 6.0});
}

TEST_CASE("Block 113 fit-point insertion removal and conversion are deterministic",
          "[core][sketch-spline-edit]") {
  auto sketch = Sketch::create(SketchId("sketch.fit"), "Fit", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.fit"), {0.0, 0.0}, {3.0, 6.0}, {7.0, 6.0}, {10.0, 0.0}).value()));

  auto model = SketchSplineEditModel::from_segments(
      sketch.value(), {SketchEntityId("spline.fit")});
  REQUIRE(model);
  REQUIRE(model.value().convert_to_fit_points());
  CHECK(model.value().representation() == SketchSplineRepresentation::FitPoints);
  REQUIRE(model.value().fit_points().size() == 3U);
  REQUIRE(model.value().insert_fit_point(2U, Point2{8.0, 4.0}));
  CHECK(model.value().segments().size() == 3U);
  CHECK(model.value().segments()[0].id().value() == "spline.fit");
  CHECK(model.value().segments()[1].id().value() == "spline.fit.fit.2");
  CHECK(model.value().segments()[2].id().value() == "spline.fit.fit.3");

  const auto first_preview = model.value().preview();
  REQUIRE(first_preview.continuity_handles.size() == 2U);
  CHECK(first_preview.continuity_handles[0].position_continuous);
  CHECK(first_preview.continuity_handles[0].tangent_continuous);

  REQUIRE(model.value().remove_fit_point(1U));
  CHECK(model.value().segments().size() == 2U);
  REQUIRE(model.value().convert_to_control_points());
  CHECK(model.value().representation() == SketchSplineRepresentation::ControlPoints);
  CHECK_FALSE(model.value().preview().handles.empty());
}

TEST_CASE("Block 113 connected spline junctions expose one endpoint and reject profile cardinality changes",
          "[core][sketch-spline-edit]") {
  auto sketch = Sketch::create(SketchId("sketch.chain"), "Chain", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.a"), {0.0, 0.0}, {2.0, 0.0}, {8.0, 5.0}, {10.0, 5.0}).value()));
  REQUIRE(sketch.value().add_entity(SplineSegment::create_cubic_bezier(
      SketchEntityId("spline.b"), {10.0, 5.0}, {12.0, 5.0}, {18.0, 0.0}, {20.0, 0.0}).value()));

  auto chain = SketchSplineEditModel::from_segments(
      sketch.value(), {SketchEntityId("spline.a"), SketchEntityId("spline.b")});
  REQUIRE(chain);
  const auto preview = chain.value().preview();
  const auto endpoint_count = std::count_if(preview.handles.begin(), preview.handles.end(),
                                            [](const auto& handle) {
                                              return handle.kind == SketchSplineHandleKind::Endpoint;
                                            });
  CHECK(endpoint_count == 3);

  PartDocument document = make_document();
  const Sketch* profile_sketch = document.find_sketch(SketchId("sketch.spline"));
  REQUIRE(profile_sketch != nullptr);
  auto profile_edit = SketchSplineEditModel::from_segments(
      *profile_sketch, {SketchEntityId("spline.right")});
  REQUIRE(profile_edit);
  REQUIRE(profile_edit.value().convert_to_fit_points());
  auto rejected = profile_edit.value().build_sketch(*profile_sketch);
  REQUIRE_FALSE(rejected);
  CHECK(rejected.error().category() == ErrorCategory::Dependency);
}

TEST_CASE("Block 113 Sketch text resolves expression parameters and roundtrips sidecar JSON",
          "[core][sketch-text]") {
  auto document = PartDocument::create(DocumentId("part.text"), "Text");
  REQUIRE(document);
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  REQUIRE(document.value().add_sketch(
      Sketch::create(SketchId("sketch.text"), "Text", DatumPlaneId("datum.xy")).value()));
  REQUIRE(document.value().add_parameter(length_parameter("text.height", "text_height", 7.0)));
  REQUIRE(document.value().add_parameter(length_parameter("part.width", "width", 40.0)));
  REQUIRE(document.value().add_expression_parameter(ParameterId("text.value"), "text_value",
                                                    ParameterType::Length, "width / 2"));

  auto text = SketchText::create(
      SketchTextId("text.width"), SketchId("sketch.text"), "WIDTH=${value}", "DIN 1451",
      ParameterId("text.height"), Point2{5.0, 8.0}, std::nullopt,
      {SketchTextBinding{"value", ParameterId("text.value"), 1U}});
  REQUIRE(text);
  auto resolved = text.value().resolve(document.value());
  REQUIRE(resolved);
  CHECK(resolved.value().text == "WIDTH=20 mm");
  CHECK(resolved.value().height_mm == Catch::Approx(7.0));

  auto catalog = SketchTextCatalog::create(document.value().id(), {text.value()});
  REQUIRE(catalog);
  auto serialized = serialize_sketch_text_catalog_to_json(catalog.value());
  REQUIRE(serialized);
  CHECK(serialized.value().find("blcad.sketch_text.mvp8") != std::string::npos);
  auto restored = deserialize_sketch_text_catalog_from_json(serialized.value());
  REQUIRE(restored);
  const SketchText* restored_text = restored.value().find(SketchTextId("text.width"));
  REQUIRE(restored_text != nullptr);
  auto restored_resolved = restored_text->resolve(document.value());
  REQUIRE(restored_resolved);
  CHECK(restored_resolved.value().text == "WIDTH=20 mm");
}
