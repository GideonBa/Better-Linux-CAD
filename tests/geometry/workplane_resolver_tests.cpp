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

void add_derived_workplane(PartDocument& document, DatumPlaneId id, const char* name,
                           SemanticFace face) {
  auto reference = SemanticFaceReference::create(FeatureId("feature.base_extrude"), face);
  REQUIRE(reference);
  auto workplane = DerivedWorkplane::create_on_feature_face(id, name, reference.value());
  REQUIRE(workplane);
  REQUIRE(document.add_derived_workplane(workplane.value()));
}

PartDocument make_face_document() {
  auto document = PartDocument::create(DocumentId("part.face_plate"), "FacePlate");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));

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

  add_derived_workplane(document.value(), DatumPlaneId("workplane.base_top"), "BaseTopFace",
                        SemanticFace::Top);
  add_derived_workplane(document.value(), DatumPlaneId("workplane.base_bottom"), "BaseBottomFace",
                        SemanticFace::Bottom);
  add_derived_workplane(document.value(), DatumPlaneId("workplane.base_right"), "BaseRightFace",
                        SemanticFace::Right);
  add_derived_workplane(document.value(), DatumPlaneId("workplane.base_left"), "BaseLeftFace",
                        SemanticFace::Left);
  add_derived_workplane(document.value(), DatumPlaneId("workplane.base_front"), "BaseFrontFace",
                        SemanticFace::Front);
  add_derived_workplane(document.value(), DatumPlaneId("workplane.base_back"), "BaseBackFace",
                        SemanticFace::Back);

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

TEST_CASE("WorkplaneResolver resolves additive-extrude top-face workplanes", "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_top"));

  REQUIRE(resolved);
  CHECK(resolved.value().origin.z == Catch::Approx(8.0));
  CHECK(resolved.value().x_axis.x == Catch::Approx(1.0));
  CHECK(resolved.value().y_axis.y == Catch::Approx(1.0));
  CHECK(resolved.value().normal.z == Catch::Approx(1.0));
  REQUIRE(resolved.value().bounds.enabled);
  CHECK(resolved.value().bounds.width_mm == Catch::Approx(120.0));
  CHECK(resolved.value().bounds.height_mm == Catch::Approx(80.0));
}

TEST_CASE("WorkplaneResolver resolves additive-extrude bottom-face workplanes", "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_bottom"));

  REQUIRE(resolved);
  CHECK(resolved.value().origin.z == Catch::Approx(0.0));
  CHECK(resolved.value().x_axis.x == Catch::Approx(1.0));
  CHECK(resolved.value().y_axis.y == Catch::Approx(1.0));
  CHECK(resolved.value().normal.z == Catch::Approx(-1.0));
  REQUIRE(resolved.value().bounds.enabled);
  CHECK(resolved.value().bounds.width_mm == Catch::Approx(120.0));
  CHECK(resolved.value().bounds.height_mm == Catch::Approx(80.0));
}

TEST_CASE("WorkplaneResolver resolves additive-extrude right-face workplanes", "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_right"));

  REQUIRE(resolved);
  CHECK(resolved.value().origin.x == Catch::Approx(60.0));
  CHECK(resolved.value().origin.y == Catch::Approx(0.0));
  CHECK(resolved.value().origin.z == Catch::Approx(4.0));
  CHECK(resolved.value().x_axis.y == Catch::Approx(1.0));
  CHECK(resolved.value().y_axis.z == Catch::Approx(1.0));
  CHECK(resolved.value().normal.x == Catch::Approx(1.0));
  REQUIRE(resolved.value().bounds.enabled);
  CHECK(resolved.value().bounds.width_mm == Catch::Approx(80.0));
  CHECK(resolved.value().bounds.height_mm == Catch::Approx(8.0));
}

TEST_CASE("WorkplaneResolver resolves additive-extrude left-face workplanes", "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_left"));

  REQUIRE(resolved);
  CHECK(resolved.value().origin.x == Catch::Approx(-60.0));
  CHECK(resolved.value().origin.y == Catch::Approx(0.0));
  CHECK(resolved.value().origin.z == Catch::Approx(4.0));
  CHECK(resolved.value().x_axis.y == Catch::Approx(-1.0));
  CHECK(resolved.value().y_axis.z == Catch::Approx(1.0));
  CHECK(resolved.value().normal.x == Catch::Approx(-1.0));
  REQUIRE(resolved.value().bounds.enabled);
  CHECK(resolved.value().bounds.width_mm == Catch::Approx(80.0));
  CHECK(resolved.value().bounds.height_mm == Catch::Approx(8.0));
}

TEST_CASE("WorkplaneResolver resolves additive-extrude front-face workplanes", "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_front"));

  REQUIRE(resolved);
  CHECK(resolved.value().origin.x == Catch::Approx(0.0));
  CHECK(resolved.value().origin.y == Catch::Approx(40.0));
  CHECK(resolved.value().origin.z == Catch::Approx(4.0));
  CHECK(resolved.value().x_axis.x == Catch::Approx(-1.0));
  CHECK(resolved.value().y_axis.z == Catch::Approx(1.0));
  CHECK(resolved.value().normal.y == Catch::Approx(1.0));
  REQUIRE(resolved.value().bounds.enabled);
  CHECK(resolved.value().bounds.width_mm == Catch::Approx(120.0));
  CHECK(resolved.value().bounds.height_mm == Catch::Approx(8.0));
}

TEST_CASE("WorkplaneResolver resolves additive-extrude back-face workplanes", "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.base_back"));

  REQUIRE(resolved);
  CHECK(resolved.value().origin.x == Catch::Approx(0.0));
  CHECK(resolved.value().origin.y == Catch::Approx(-40.0));
  CHECK(resolved.value().origin.z == Catch::Approx(4.0));
  CHECK(resolved.value().x_axis.x == Catch::Approx(1.0));
  CHECK(resolved.value().y_axis.z == Catch::Approx(1.0));
  CHECK(resolved.value().normal.y == Catch::Approx(-1.0));
  REQUIRE(resolved.value().bounds.enabled);
  CHECK(resolved.value().bounds.width_mm == Catch::Approx(120.0));
  CHECK(resolved.value().bounds.height_mm == Catch::Approx(8.0));
}

TEST_CASE("WorkplaneResolver maps side-face local sketch points through resolved frames",
          "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto right = resolver.resolve(document, DatumPlaneId("workplane.base_right"));
  REQUIRE(right);
  const Point3 right_global = resolver.evaluate_point(right.value(), Point2{-12.0, 1.5});
  CHECK(right_global.x == Catch::Approx(60.0));
  CHECK(right_global.y == Catch::Approx(-12.0));
  CHECK(right_global.z == Catch::Approx(5.5));

  const auto left = resolver.resolve(document, DatumPlaneId("workplane.base_left"));
  REQUIRE(left);
  const Point3 left_global = resolver.evaluate_point(left.value(), Point2{-12.0, 1.5});
  CHECK(left_global.x == Catch::Approx(-60.0));
  CHECK(left_global.y == Catch::Approx(12.0));
  CHECK(left_global.z == Catch::Approx(5.5));

  const auto front = resolver.resolve(document, DatumPlaneId("workplane.base_front"));
  REQUIRE(front);
  const Point3 front_global = resolver.evaluate_point(front.value(), Point2{-12.0, 1.5});
  CHECK(front_global.x == Catch::Approx(12.0));
  CHECK(front_global.y == Catch::Approx(40.0));
  CHECK(front_global.z == Catch::Approx(5.5));

  const auto back = resolver.resolve(document, DatumPlaneId("workplane.base_back"));
  REQUIRE(back);
  const Point3 back_global = resolver.evaluate_point(back.value(), Point2{-12.0, 1.5});
  CHECK(back_global.x == Catch::Approx(-12.0));
  CHECK(back_global.y == Catch::Approx(-40.0));
  CHECK(back_global.z == Catch::Approx(5.5));
}

TEST_CASE("WorkplaneResolver rejects missing workplanes", "[geometry][workplane]") {
  const PartDocument document = make_face_document();
  const WorkplaneResolver resolver;

  const auto resolved = resolver.resolve(document, DatumPlaneId("workplane.missing"));

  REQUIRE(resolved.has_error());
  CHECK(resolved.error().category() == ErrorCategory::Validation);
  CHECK(resolved.error().message() == "workplane must exist in part document");
}
