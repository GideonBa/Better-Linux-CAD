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

PartDocument make_top_face_document(double width_mm, double height_mm, double depth_mm) {
  auto document = PartDocument::create(DocumentId("part.top_face_motion"), "Top Face Motion");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", width_mm)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", height_mm)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", depth_mm)));
  auto datum = DatumPlane::xy();
  REQUIRE(datum);
  REQUIRE(document.value().add_datum_plane(datum.value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"), Point2{0.0, 0.0});
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));
  auto feature = Feature::create_additive_extrude(FeatureId("feature.base"), "BaseExtrude",
                                                  SketchId("sketch.base"),
                                                  ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  auto face = SemanticFaceReference::create(FeatureId("feature.base"), SemanticFace::Top);
  REQUIRE(face);
  auto workplane = DerivedWorkplane::create_on_feature_face(DatumPlaneId("workplane.top"),
                                                            "Top", face.value());
  REQUIRE(workplane);
  REQUIRE(document.value().add_derived_workplane(workplane.value()));
  return document.value();
}

} // namespace

TEST_CASE("Generated-face sketch workplane origin follows source feature dimensions",
          "[geometry][workplane][recovery]") {
  PartDocument document = make_top_face_document(100.0, 60.0, 10.0);
  const WorkplaneResolver resolver;

  auto initial = resolver.resolve(document, DatumPlaneId("workplane.top"));
  REQUIRE(initial);
  CHECK(initial.value().origin.x == Catch::Approx(0.0));
  CHECK(initial.value().origin.y == Catch::Approx(0.0));
  CHECK(initial.value().origin.z == Catch::Approx(10.0));

  auto quantity = Quantity::length_mm(25.0, "part.depth");
  REQUIRE(quantity);
  auto affected = document.set_parameter_value(ParameterId("part.depth"), quantity.value());
  REQUIRE(affected);

  auto moved = resolver.resolve(document, DatumPlaneId("workplane.top"));
  REQUIRE(moved);
  CHECK(moved.value().origin.x == Catch::Approx(0.0));
  CHECK(moved.value().origin.y == Catch::Approx(0.0));
  CHECK(moved.value().origin.z == Catch::Approx(25.0));
}

TEST_CASE("Reference recovery evaluator reports missing generated source instead of remapping",
          "[geometry][workplane][recovery]") {
  PartDocument document = make_top_face_document(100.0, 60.0, 10.0);
  const ReferenceRecoveryEvaluator evaluator;
  auto missing_face = SemanticReferenceTarget::create_face(FeatureId("feature.deleted"),
                                                           SemanticFace::Top);
  REQUIRE(missing_face);

  auto status = evaluator.evaluate(ReferenceStatusId("status.deleted_top"), document,
                                   missing_face.value());
  REQUIRE(status);
  CHECK(status.value().status() == ReferenceStatusKind::Lost);
  CHECK(status.value().target().node_id() == "feature.deleted.face.top");
}
