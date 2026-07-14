#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
using json = nlohmann::json;

namespace {

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PathCurve path_curve() {
  auto segment = PathSegmentReference::create_planar(
      SketchId("sketch.path"), SketchEntityId("line.path"), PlanarPathCurveKind::Line);
  REQUIRE(segment);
  auto path = PathCurve::create(PathCurveId("path.route"), "Route", {segment.value()},
                                PathClosure::Open, PathOrientationRule::MinimumTwist);
  REQUIRE(path);
  return path.value();
}

PartDocument document_with_path() {
  auto created = PartDocument::create(DocumentId("part.path-extrude"), "Path extrude");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.add_parameter(length_parameter("base.depth", 10.0)));
  auto profile = Sketch::create(SketchId("sketch.profile"), "Profile", DatumPlaneId("datum.xy"));
  auto base = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  auto path = Sketch::create(SketchId("sketch.path"), "Path", DatumPlaneId("datum.xy"));
  REQUIRE(profile);
  REQUIRE(base);
  REQUIRE(path);
  auto line = LineSegment::create(SketchEntityId("line.path"), {0.0, 0.0}, {10.0, 0.0});
  REQUIRE(line);
  REQUIRE(path.value().add_entity(line.value()));
  REQUIRE(document.add_sketch(profile.value()));
  REQUIRE(document.add_sketch(base.value()));
  REQUIRE(document.add_sketch(path.value()));
  REQUIRE(document.add_path_curve(path_curve()));
  auto base_feature = Feature::create_additive_extrude(
      FeatureId("feature.base"), "Base", SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base_feature);
  REQUIRE(document.add_feature(base_feature.value()));
  return document;
}

} // namespace

TEST_CASE("Block 83 freezes path direction on existing Extrude feature families",
          "[core][path-extrude]") {
  auto additive =
      Feature::create_additive_path_extrude(FeatureId("feature.path-add"), "Path add",
                                            SketchId("sketch.profile"), PathCurveId("path.route"));
  auto subtractive = Feature::create_subtractive_path_extrude(
      FeatureId("feature.path-cut"), "Path cut", SketchId("sketch.profile"),
      FeatureId("feature.base"), PathCurveId("path.route"));
  REQUIRE(additive);
  REQUIRE(subtractive);
  CHECK(additive.value().type() == FeatureType::AdditiveExtrude);
  CHECK(subtractive.value().type() == FeatureType::SubtractiveExtrude);
  CHECK(additive.value().direction() == ExtrudeDirection::Path);
  CHECK(additive.value().path_curve() == PathCurveId("path.route"));
  CHECK(to_string(ExtrudeDirection::Path) == "path");
  CHECK(Feature::create_additive_path_extrude(FeatureId("bad"), "Bad", SketchId("sketch.profile"),
                                              PathCurveId())
            .has_error());
}

TEST_CASE("Block 83 tracks and roundtrips path Extrude dependencies",
          "[core][path-extrude][integration][part-construction-mvp]") {
  auto document = document_with_path();
  auto additive =
      Feature::create_additive_path_extrude(FeatureId("feature.path-add"), "Path add",
                                            SketchId("sketch.profile"), PathCurveId("path.route"));
  REQUIRE(additive);
  REQUIRE(document.add_feature(additive.value()));
  CHECK(document.dependency_graph().has_dependency("path.route", "feature.path-add"));
  CHECK(document.remove_path_curve(PathCurveId("path.route")).has_error());
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const json value = json::parse(serialized.value());
  CHECK(value["features"][1]["direction"] == "path");
  CHECK(value["features"][1]["path_curve"] == "path.route");
  CHECK_FALSE(value["features"][1].contains("extrude"));
  auto restored = deserialize_part_document_from_json(serialized.value());
  INFO((restored.has_error() ? restored.error().object_id() + ": " + restored.error().message()
                             : std::string("round-trip succeeded")));
  REQUIRE(restored);
  const Feature* feature = restored.value().find_feature(FeatureId("feature.path-add"));
  REQUIRE(feature != nullptr);
  CHECK(feature->path_curve() == PathCurveId("path.route"));

  json malformed = value;
  malformed["features"][1].erase("path_curve");
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
}
