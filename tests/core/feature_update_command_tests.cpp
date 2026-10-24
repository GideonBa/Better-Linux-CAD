#include "blcad/core/edge_treatment_feature.hpp"
#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace blcad;

namespace {

Parameter length(const char* id, double value) {
  return Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value, id).value()).value();
}

// A Part with a base extrude (feature.base -> body.base) and a fillet on that
// body (feature.fillet), so update/remove can be exercised with a real
// downstream dependency.
PartDocument document(bool with_fillet = true) {
  auto part = PartDocument::create(DocumentId("part.update"), "Update").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  for (auto parameter : {length("width", 20.0), length("height", 10.0), length("depth", 5.0),
                         length("depth2", 8.0), length("radius", 1.0)})
    REQUIRE(part.add_parameter(std::move(parameter)));
  auto sketch = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_profile(RectangleProfile::create(ProfileId("profile.base"),
                                                      ParameterId("width"),
                                                      ParameterId("height")).value()));
  REQUIRE(part.add_sketch(std::move(sketch)));
  REQUIRE(part.add_body(Body::create(BodyId("body.base"), "Base", BodyKind::Solid).value()));
  REQUIRE(part.add_feature(
      Feature::create_additive_extrude(FeatureId("feature.base"), "Base", SketchId("sketch.base"),
                                       ParameterId("depth"))
          .value()
          .with_body_result_context(
              FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                               BodyId("body.base")).value())
          .value()));
  if (!with_fillet)
    return part;
  auto edge = EdgeReference::create_linear(
                  PartFeatureInputRole::FilletEdge,
                  SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront)
                      .value())
                  .value();
  REQUIRE(part.add_fillet_feature(
      FilletFeature::create(FeatureId("feature.fillet"), "Fillet", BodyId("body.base"),
                            std::vector<EdgeReference>{edge}, ParameterId("radius")).value()));
  return part;
}

Feature extrude_with_depth(const char* depth_parameter) {
  return Feature::create_additive_extrude(FeatureId("feature.base"), "Base", SketchId("sketch.base"),
                                          ParameterId(depth_parameter))
      .value()
      .with_body_result_context(
          FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                           BodyId("body.base")).value())
      .value();
}

} // namespace

TEST_CASE("Block 129 update_feature replaces intent in place, keeping id and position",
          "[core][feature-update-command]") {
  PartDocument part = document();
  const std::string before = serialize_part_document_to_json(part).value();
  CHECK(part.features().size() == 1);
  CHECK(part.find_feature(FeatureId("feature.base"))->length_parameter().value() == "depth");

  REQUIRE(part.update_feature(extrude_with_depth("depth2")));
  // Same id, same ordered position, new bound parameter, downstream fillet intact.
  CHECK(part.features().size() == 1);
  CHECK(part.features().front().id().value() == "feature.base");
  CHECK(part.find_feature(FeatureId("feature.base"))->length_parameter().value() == "depth2");
  CHECK(part.fillet_features().size() == 1);
  REQUIRE(part.create_recompute_plan());

  // The JSON shape is stable except the edited binding; ids do not churn.
  const std::string after = serialize_part_document_to_json(part).value();
  CHECK(before != after);
  CHECK(after.find("feature.base") != std::string::npos);
  CHECK(after.find("feature.fillet") != std::string::npos);
}

TEST_CASE("Block 129 update_feature fails closed on unknown id and invalid replacement",
          "[core][feature-update-command]") {
  PartDocument part = document();
  const std::string before = serialize_part_document_to_json(part).value();

  auto missing = Feature::create_additive_extrude(FeatureId("feature.ghost"), "Ghost",
                                                  SketchId("sketch.base"), ParameterId("depth"))
                     .value();
  CHECK(part.update_feature(missing).has_error());  // id not present

  auto invalid = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                                  SketchId("sketch.missing"), ParameterId("depth"))
                     .value();
  CHECK(part.update_feature(invalid).has_error());  // replacement references a missing sketch
  CHECK(serialize_part_document_to_json(part).value() == before);  // no partial mutation
}

TEST_CASE("Block 129 update_fillet_feature rebinds an edge-treatment parameter with a stable id",
          "[core][feature-update-command]") {
  PartDocument part = document();
  auto edge = EdgeReference::create_linear(
                  PartFeatureInputRole::FilletEdge,
                  SemanticEdgeReference::create(FeatureId("feature.base"), SemanticEdge::TopFront)
                      .value())
                  .value();
  REQUIRE(part.add_parameter(length("radius2", 2.0)));
  auto rebonded = FilletFeature::create(FeatureId("feature.fillet"), "Fillet", BodyId("body.base"),
                                        std::vector<EdgeReference>{edge}, ParameterId("radius2"))
                      .value();
  REQUIRE(part.update_fillet_feature(rebonded));
  CHECK(part.fillet_features().size() == 1);
  CHECK(part.find_fillet_feature(FeatureId("feature.fillet"))->radius_parameter().value() ==
        "radius2");
  REQUIRE(part.create_recompute_plan());
}

TEST_CASE("Feature updates preserve every position in an in-place Revolve Body chain",
          "[core][feature-update-command][revolve-update-history]") {
  auto part = document(false);
  REQUIRE(part.add_datum_axis(
      DatumAxis::create_explicit(DatumAxisId("axis.x"), "X", {}, {1, 0, 0}).value()));
  REQUIRE(part.add_datum_axis(
      DatumAxis::create_explicit(DatumAxisId("axis.y"), "Y", {}, {0, 1, 0}).value()));
  const auto make = [&](const std::string& id, double angle, const char* axis) {
    return RevolveFeature::create_revolve_cut(
               FeatureId(id), id,
               ProfileRegionReference::create(SketchId("sketch.base"), ProfileId("profile.base"),
                                              PartFeatureInputRole::RevolveProfile)
                   .value(),
               AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis,
                                                DatumAxisId(axis))
                   .value(),
               RevolveAngleExtent::angle(angle, RevolveSide::Negative).value(),
               FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut, BodyId("body.base"),
                                                std::nullopt)
                   .value())
        .value();
  };
  for (int i = 1; i <= 3; ++i)
    REQUIRE(part.add_revolve_feature(make("revolve." + std::to_string(i), 90, "axis.x")));
  REQUIRE(part.dependency_graph().has_dependency("feature.base", "revolve.1"));
  for (int i = 1; i <= 3; ++i) {
    const auto id = "revolve." + std::to_string(i);
    REQUIRE(part.update_revolve_feature(make(id, 60, "axis.y")));
    const auto& graph = part.dependency_graph();
    CHECK(graph.has_dependency(i == 1 ? "feature.base" : "revolve." + std::to_string(i - 1), id));
    CHECK(graph.has_dependency("revolve.3", "body:body.base"));
    CHECK_FALSE(graph.has_dependency("revolve.1", "body:body.base"));
    CHECK_FALSE(graph.has_dependency("revolve.2", "body:body.base"));
    CHECK(graph.has_dependency("revolve.1", "revolve.2"));
    CHECK(graph.has_dependency("revolve.2", "revolve.3"));
    CHECK(graph.has_dependency("axis.y", id));
    CHECK_FALSE(graph.has_dependency("axis.x", id));
    REQUIRE(part.create_recompute_plan());
    const auto text = serialize_part_document_to_json(part);
    REQUIRE(text);
    auto reopened = deserialize_part_document_from_json(text.value());
    REQUIRE(reopened);
    auto reopened_edges = reopened.value().dependency_graph().dependencies();
    auto original_edges = graph.dependencies();
    std::sort(reopened_edges.begin(), reopened_edges.end());
    std::sort(original_edges.begin(), original_edges.end());
    CHECK(reopened_edges == original_edges);
    CHECK_FALSE(part.update_revolve_feature(make(id, 80, "axis.missing")));
    CHECK(serialize_part_document_to_json(part).value() == text.value());
  }
}

TEST_CASE("Block 129 remove_feature is blocked by downstream dependents and succeeds otherwise",
          "[core][feature-update-command]") {
  PartDocument part = document();
  // The base extrude cannot be removed while the fillet depends on its body.
  CHECK(part.remove_feature(FeatureId("feature.base")).has_error());
  CHECK(part.features().size() == 1);

  // The fillet has nothing downstream and removes cleanly.
  REQUIRE(part.remove_fillet_feature(FeatureId("feature.fillet")));
  CHECK(part.fillet_features().empty());

  // With the dependent gone, the base extrude now removes too.
  REQUIRE(part.remove_feature(FeatureId("feature.base")));
  CHECK(part.features().empty());
  REQUIRE(part.create_recompute_plan());

  CHECK(part.remove_feature(FeatureId("feature.base")).has_error());  // already gone
}
