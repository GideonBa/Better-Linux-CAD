#include "blcad/core/part_document.hpp"
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

void add_line(Sketch& sketch, const char* id, Point2 start, Point2 end) {
  auto line = LineSegment::create(SketchEntityId(id), start, end);
  REQUIRE(line);
  REQUIRE(sketch.add_entity(line.value()));
}

Sketch make_composite_sketch() {
  auto sketch = Sketch::create(SketchId("sketch.composite"), "Composite", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  add_line(sketch.value(), "outer.bottom", Point2{-20.0, -10.0}, Point2{20.0, -10.0});
  add_line(sketch.value(), "outer.right", Point2{20.0, -10.0}, Point2{20.0, 10.0});
  add_line(sketch.value(), "outer.top", Point2{20.0, 10.0}, Point2{-20.0, 10.0});
  add_line(sketch.value(), "outer.left", Point2{-20.0, 10.0}, Point2{-20.0, -10.0});
  add_line(sketch.value(), "inner.bottom", Point2{-5.0, -3.0}, Point2{5.0, -3.0});
  add_line(sketch.value(), "inner.right", Point2{5.0, -3.0}, Point2{5.0, 3.0});
  add_line(sketch.value(), "inner.top", Point2{5.0, 3.0}, Point2{-5.0, 3.0});
  add_line(sketch.value(), "inner.left", Point2{-5.0, 3.0}, Point2{-5.0, -3.0});
  auto profile = CompositeClosedProfile::create(
      ProfileId("profile.composite"),
      {SketchEntityId("outer.bottom"), SketchEntityId("outer.right"), SketchEntityId("outer.top"),
       SketchEntityId("outer.left")},
      {{SketchEntityId("inner.bottom"), SketchEntityId("inner.right"), SketchEntityId("inner.top"),
        SketchEntityId("inner.left")}});
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

PartDocument make_document() {
  auto document = PartDocument::create(DocumentId("part.composite.json"), "CompositeJson");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 5.0)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  REQUIRE(document.value().add_sketch(make_composite_sketch()));
  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.composite"), "Composite",
                                       SketchId("sketch.composite"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

} // namespace

TEST_CASE("Composite closed profiles roundtrip through JSON", "[core][json][composite-profile]") {
  PartDocument document = make_document();
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  CHECK(serialized.value().find("composite_closed_profiles") != std::string::npos);
  CHECK(serialized.value().find("outer_contour") != std::string::npos);
  CHECK(serialized.value().find("inner_contours") != std::string::npos);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const Sketch* sketch = restored.value().find_sketch(SketchId("sketch.composite"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->composite_closed_profiles().size() == 1U);
  const CompositeClosedProfile& profile = sketch->composite_closed_profiles().front();
  CHECK(profile.id() == ProfileId("profile.composite"));
  CHECK(profile.outer_contour().size() == 4U);
  REQUIRE(profile.inner_contours().size() == 1U);
  CHECK(profile.inner_contours().front().size() == 4U);
}
