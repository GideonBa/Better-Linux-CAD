#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Parameter make_count_parameter(const char* id, const char* name, double value) {
  auto quantity = Quantity::count(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_count(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

CircularHolePattern make_pattern() {
  auto pattern = CircularHolePattern::create(
      ProfileId("pattern.bolt_circle"), ParameterId("part.bolt_circle_radius"),
      ParameterId("part.bolt_count"), ParameterId("part.bolt_hole_diameter"));
  REQUIRE(pattern);
  return pattern.value();
}

PartDocument make_document_with_pattern_parameters() {
  auto document = PartDocument::create(DocumentId("part.flange"), "Flange");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.bolt_circle_radius", "bolt_circle_radius", 30.0)));
  REQUIRE(document.value().add_parameter(make_count_parameter("part.bolt_count", "bolt_count", 6)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.bolt_hole_diameter", "bolt_hole_diameter", 6.6)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));
  return document.value();
}

Sketch make_pattern_sketch() {
  auto sketch =
      Sketch::create(SketchId("sketch.bolt_circle"), "Sketch_BoltCircle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_profile(make_pattern()));
  return sketch.value();
}

} // namespace

TEST_CASE("Quantity count accepts whole numbers of at least one") {
  auto count = Quantity::count(8.0);
  REQUIRE(count);
  CHECK(count.value().kind() == QuantityKind::Count);
  CHECK(count.value().count_value() == 8U);
  CHECK(count.value().unit() == "1");
  CHECK(count.value().is_valid_count());
  CHECK_FALSE(count.value().is_positive_length());
}

TEST_CASE("Quantity count rejects invalid values") {
  CHECK(Quantity::count(0.0).has_error());
  CHECK(Quantity::count(-3.0).has_error());
  CHECK(Quantity::count(2.5).has_error());
  CHECK(Quantity::count(std::numeric_limits<double>::infinity()).has_error());
  CHECK(Quantity::count(std::numeric_limits<double>::quiet_NaN()).has_error());
}

TEST_CASE("Parameter create_count stores a count parameter") {
  const Parameter parameter = make_count_parameter("part.bolt_count", "bolt_count", 6);
  CHECK(parameter.type() == ParameterType::Count);
  CHECK(parameter.value().count_value() == 6U);
  CHECK(to_string(parameter.type()) == "count");
}

TEST_CASE("Parameter create_count rejects a length quantity") {
  auto length = Quantity::length_mm(6.0);
  REQUIRE(length);
  CHECK(Parameter::create_count(ParameterId("part.bolt_count"), "bolt_count", length.value())
            .has_error());
}

TEST_CASE("Parameter create_length rejects a count quantity") {
  auto count = Quantity::count(6.0);
  REQUIRE(count);
  CHECK(Parameter::create_length(ParameterId("part.width"), "width", count.value()).has_error());
}

TEST_CASE("CircularHolePattern validates its inputs") {
  CHECK(CircularHolePattern::create(ProfileId(""), ParameterId("r"), ParameterId("n"),
                                    ParameterId("d"))
            .has_error());
  CHECK(CircularHolePattern::create(ProfileId("p"), ParameterId(""), ParameterId("n"),
                                    ParameterId("d"))
            .has_error());
  CHECK(CircularHolePattern::create(ProfileId("p"), ParameterId("r"), ParameterId(""),
                                    ParameterId("d"))
            .has_error());
  CHECK(CircularHolePattern::create(ProfileId("p"), ParameterId("r"), ParameterId("n"),
                                    ParameterId(""))
            .has_error());
  CHECK(CircularHolePattern::create(ProfileId("p"), ParameterId("r"), ParameterId("n"),
                                    ParameterId("d"), Point2{},
                                    std::numeric_limits<double>::infinity())
            .has_error());

  const CircularHolePattern pattern = make_pattern();
  CHECK(pattern.id().value() == "pattern.bolt_circle");
  CHECK(pattern.angle_offset_deg() == 0.0);
}

TEST_CASE("Sketch rejects duplicate profile ids across profile kinds") {
  auto sketch = Sketch::create(SketchId("sketch.test"), "Sketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto circle = CircleProfile::create(ProfileId("pattern.bolt_circle"), ParameterId("d"));
  REQUIRE(circle);
  REQUIRE(sketch.value().add_profile(circle.value()));
  CHECK(sketch.value().add_profile(make_pattern()).has_error());
}

TEST_CASE("PartDocument validates circular hole pattern parameters") {
  SECTION("missing radius parameter") {
    auto document = PartDocument::create(DocumentId("part.flange"), "Flange");
    REQUIRE(document);
    REQUIRE(
        document.value().add_parameter(make_count_parameter("part.bolt_count", "bolt_count", 6)));
    REQUIRE(document.value().add_parameter(
        make_length_parameter("part.bolt_hole_diameter", "bolt_hole_diameter", 6.6)));
    auto xy = DatumPlane::xy();
    REQUIRE(xy);
    REQUIRE(document.value().add_datum_plane(xy.value()));
    CHECK(document.value().add_sketch(make_pattern_sketch()).has_error());
  }

  SECTION("count parameter must be a count parameter") {
    auto document = PartDocument::create(DocumentId("part.flange"), "Flange");
    REQUIRE(document);
    REQUIRE(document.value().add_parameter(
        make_length_parameter("part.bolt_circle_radius", "bolt_circle_radius", 30.0)));
    REQUIRE(document.value().add_parameter(
        make_length_parameter("part.bolt_count", "bolt_count", 6.0)));
    REQUIRE(document.value().add_parameter(
        make_length_parameter("part.bolt_hole_diameter", "bolt_hole_diameter", 6.6)));
    auto xy = DatumPlane::xy();
    REQUIRE(xy);
    REQUIRE(document.value().add_datum_plane(xy.value()));
    const auto added = document.value().add_sketch(make_pattern_sketch());
    REQUIRE(added.has_error());
    CHECK(added.error().message() == "circular hole pattern count parameter must be "
                                     "a count parameter in part document");
  }
}

TEST_CASE("PartDocument connects circular hole pattern parameters to the sketch") {
  PartDocument document = make_document_with_pattern_parameters();
  REQUIRE(document.add_sketch(make_pattern_sketch()));

  const DependencyGraph& graph = document.dependency_graph();
  CHECK(graph.has_dependency("part.bolt_circle_radius", "sketch.bolt_circle"));
  CHECK(graph.has_dependency("part.bolt_count", "sketch.bolt_circle"));
  CHECK(graph.has_dependency("part.bolt_hole_diameter", "sketch.bolt_circle"));
}

TEST_CASE("Changing the bolt count marks the pattern sketch and cut feature") {
  PartDocument document = make_document_with_pattern_parameters();
  REQUIRE(document.add_sketch(make_pattern_sketch()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.add_sketch(base_sketch.value()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.bolt_circle_cut"),
                                                 "BoltCircleCut", SketchId("sketch.bolt_circle"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.add_feature(cut.value()));
  document.mark_all_clean();

  auto new_count = Quantity::count(12.0, "part.bolt_count");
  REQUIRE(new_count);
  auto affected = document.set_parameter_value(ParameterId("part.bolt_count"), new_count.value());
  REQUIRE(affected);

  const std::vector<std::string>& nodes = affected.value();
  CHECK(std::find(nodes.begin(), nodes.end(), "sketch.bolt_circle") != nodes.end());
  CHECK(std::find(nodes.begin(), nodes.end(), "feature.bolt_circle_cut") != nodes.end());
  CHECK(document.find_parameter(ParameterId("part.bolt_count"))->value().count_value() == 12U);
}

TEST_CASE("Circular hole pattern and count parameter survive JSON roundtrip") {
  PartDocument document = make_document_with_pattern_parameters();
  REQUIRE(document.add_sketch(make_pattern_sketch()));

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);

  const Parameter* count = restored.value().find_parameter(ParameterId("part.bolt_count"));
  REQUIRE(count != nullptr);
  CHECK(count->type() == ParameterType::Count);
  CHECK(count->value().count_value() == 6U);
  CHECK(count->value().unit() == "1");

  const Sketch* sketch = restored.value().find_sketch(SketchId("sketch.bolt_circle"));
  REQUIRE(sketch != nullptr);
  REQUIRE(sketch->circular_hole_patterns().size() == 1U);
  const CircularHolePattern& pattern = sketch->circular_hole_patterns().front();
  CHECK(pattern.id().value() == "pattern.bolt_circle");
  CHECK(pattern.radius_parameter().value() == "part.bolt_circle_radius");
  CHECK(pattern.count_parameter().value() == "part.bolt_count");
  CHECK(pattern.hole_diameter_parameter().value() == "part.bolt_hole_diameter");
  CHECK(pattern.angle_offset_deg() == 0.0);

  const DependencyGraph& graph = restored.value().dependency_graph();
  CHECK(graph.has_dependency("part.bolt_count", "sketch.bolt_circle"));
}
