#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);

  return parameter.value();
}

PartDocument make_triangle_prism_document() {
  auto document = PartDocument::create(DocumentId("part.triangle_prism"), "TrianglePrism");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 10.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto sketch = Sketch::create(SketchId("sketch.triangle"), "Sketch_Triangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);

  REQUIRE(sketch.value().add_entity(LineSegment::create(SketchEntityId("line.a"), Point2{0.0, 0.0},
                                                        Point2{20.0, 0.0})
                                        .value()));
  REQUIRE(sketch.value().add_entity(LineSegment::create(SketchEntityId("line.b"), Point2{20.0, 0.0},
                                                        Point2{0.0, 20.0})
                                        .value()));
  REQUIRE(sketch.value().add_entity(LineSegment::create(SketchEntityId("line.c"), Point2{0.0, 20.0},
                                                        Point2{0.0, 0.0})
                                        .value()));

  auto profile = ClosedProfile::create(ProfileId("profile.triangle"),
                                       {SketchEntityId("line.a"), SketchEntityId("line.b"),
                                        SketchEntityId("line.c")});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto feature = Feature::create_additive_extrude(FeatureId("feature.triangle_prism"),
                                                  "TrianglePrism", SketchId("sketch.triangle"),
                                                  ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  return document.value();
}

} // namespace

TEST_CASE("PartDocument JSON round-trips line-based closed profiles", "[core][json][closed_profile]") {
  const PartDocument document = make_triangle_prism_document();

  const auto serialized = serialize_part_document_to_json(document);

  REQUIRE(serialized);
  CHECK(serialized.value().find("line_segments") != std::string::npos);
  CHECK(serialized.value().find("closed_profiles") != std::string::npos);
  CHECK(serialized.value().find("profile.triangle") != std::string::npos);

  const auto restored = deserialize_part_document_from_json(serialized.value());

  REQUIRE(restored);
  CHECK(restored.value().sketch_count() == 1);
  CHECK(restored.value().feature_count() == 1);

  const auto* sketch = restored.value().find_sketch(SketchId("sketch.triangle"));
  REQUIRE(sketch != nullptr);
  CHECK(sketch->line_segments().size() == 3);
  CHECK(sketch->closed_profiles().size() == 1);

  const auto vertices = sketch->closed_profile_vertices(sketch->closed_profiles().front());
  REQUIRE(vertices);
  CHECK(vertices.value().size() == 3);

  CHECK(restored.value().dependency_graph().has_dependency("sketch.triangle",
                                                          "feature.triangle_prism"));
  CHECK(restored.value().dependency_graph().has_dependency("part.depth", "feature.triangle_prism"));
}
