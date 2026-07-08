#include "blcad/geometry/workplane_resolver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);

  return parameter.value();
}

PartDocument make_face_document() {
  auto document = PartDocument::create(DocumentId("part.face_plate"), "FacePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));

  auto base = Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                               SketchId("sketch.base"),
                                               ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto top_reference = SemanticFaceReference::create(FeatureId("feature.base_extrude"),
                                                     SemanticFace::Top);
  REQUIRE(top_reference);
  auto top_workplane = DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("workplane.base_top"), "BaseTopFace", top_reference.value());
  REQUIRE(top_workplane);
  REQUIRE(document.value().add_derived_workplane(top_workplane.value()));

  auto bottom_reference = SemanticFaceReference::create(FeatureId("feature.base_extrude"),
                                                        SemanticFace::Bottom);
  REQUIRE(bottom_reference);
  auto bottom_workplane = DerivedWorkplane::create_on_feature_face(
      DatumPlaneId("workplane.base_bottom"), "BaseBottomFace", bottom_reference.value());
  REQUIRE(bottom_workplane);
  REQUIRE(document.value().add_derived_workplane(bottom_workplane.value()));

  return document.value();
}

} // namespace

TEST_CASE("WorkplaneResolver resolves standard datum planes", "[geometry][workplane]") {
  auto document = PartDocument::create(DocumentId("part.datum"), "DatumPart");
  REQUIRE(document);

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  const WorkplaneResolver resolver;
  const auto resolved = resolver.resolve(document.value(), DatumPlaneId("datum.xy"));

  REQUIRE(resolved);
  CHECK(resolved.value().id.value() == "datum.xy");
  CHECK(resolved.value().origin.x == Catch::Approx(0.0));
  CHECK(resolved.value().origin.y == Catch::Approx(0.0));
  CHECK(resolved.value().origin.z == Catch::Approx(0.0));
  CHECK(resolved.value().normal.z == Catch::Approx(1.0));
  CHECK_FALSE(resolved.value().bounds.enabled);
}

TEST_CASE("WorkplaneResolver resolves additive-extrude top-face workplanes",
          "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_top"));

  REQUIRE(resolved);
  CHECK(resolved.value().id.value() == "workplane.base_top");
  CHECK(resolved.value().origin.x == Catch::Approx(0.0));
  CHECK(resolved.value().origin.y == Catch::Approx(0.0));
  CHECK(resolved.value().origin.z == Catch::Approx(8.0));
  CHECK(resolved.value().x_axis.x == Catch::Approx(1.0));
  CHECK(resolved.value().y_axis.y == Catch::Approx(1.0));
  CHECK(resolved.value().normal.z == Catch::Approx(1.0));
  REQUIRE(resolved.value().bounds.enabled);
  CHECK(resolved.value().bounds.center.x == Catch::Approx(0.0));
  CHECK(resolved.value().bounds.center.y == Catch::Approx(0.0));
  CHECK(resolved.value().bounds.width_mm == Catch::Approx(120.0));
  CHECK(resolved.value().bounds.height_mm == Catch::Approx(80.0));
}

TEST_CASE("WorkplaneResolver resolves additive-extrude bottom-face workplanes",
          "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_bottom"));

  REQUIRE(resolved);
  CHECK(resolved.value().id.value() == "workplane.base_bottom");
  CHECK(resolved.value().origin.x == Catch::Approx(0.0));
  CHECK(resolved.value().origin.y == Catch::Approx(0.0));
  CHECK(resolved.value().origin.z == Catch::Approx(0.0));
  CHECK(resolved.value().x_axis.x == Catch::Approx(1.0));
  CHECK(resolved.value().y_axis.y == Catch::Approx(1.0));
  CHECK(resolved.value().normal.z == Catch::Approx(-1.0));
  REQUIRE(resolved.value().bounds.enabled);
  CHECK(resolved.value().bounds.center.x == Catch::Approx(0.0));
  CHECK(resolved.value().bounds.center.y == Catch::Approx(0.0));
  CHECK(resolved.value().bounds.width_mm == Catch::Approx(120.0));
  CHECK(resolved.value().bounds.height_mm == Catch::Approx(80.0));
}

TEST_CASE("WorkplaneResolver maps top-face local sketch points through the resolved frame",
          "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_top"));
  REQUIRE(resolved);

  const Point3 global = resolver.evaluate_point(resolved.value(), Point2{25.0, -10.0});

  CHECK(global.x == Catch::Approx(25.0));
  CHECK(global.y == Catch::Approx(-10.0));
  CHECK(global.z == Catch::Approx(8.0));
}

TEST_CASE("WorkplaneResolver maps bottom-face local sketch points through the resolved frame",
          "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_bottom"));
  REQUIRE(resolved);

  const Point3 global = resolver.evaluate_point(resolved.value(), Point2{-15.0, 12.0});

  CHECK(global.x == Catch::Approx(-15.0));
  CHECK(global.y == Catch::Approx(12.0));
  CHECK(global.z == Catch::Approx(0.0));
}

TEST_CASE("WorkplaneResolver rejects missing workplanes", "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.missing"));

  REQUIRE(resolved.has_error());
  CHECK(resolved.error().category() == ErrorCategory::Validation);
  CHECK(resolved.error().message() == "workplane must exist in part document");
}
