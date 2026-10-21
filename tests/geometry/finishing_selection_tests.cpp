#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace blcad;
using namespace blcad::geometry;

namespace {
PartDocument drawn_box() {
  auto part = PartDocument::create(DocumentId("drawn.box"), "Drawn box").value();
  REQUIRE(part.add_construction_plane(
      ConstructionPlane::create_explicit(ConstructionPlaneId("plane"), "Offset YZ", {40, 50, 70},
                                         {0, 1, 0}, {0, 0, 1}, {1, 0, 0})
          .value()));
  REQUIRE(part.add_parameter(
      Parameter::create_length(ParameterId("depth"), "Depth", Quantity::length_mm(10).value())
          .value()));
  REQUIRE(part.add_parameter(
      Parameter::create_length(ParameterId("radius"), "Radius", Quantity::length_mm(1).value())
          .value()));
  auto sketch = Sketch::create(SketchId("rectangle"), "Rectangle", DatumPlaneId("plane")).value();
  std::vector<Point2> points{{-30, -20}, {-10, -20}, {-10, -8}, {-30, -8}};
  std::vector<SketchEntityId> edges;
  for (std::size_t i = 0; i < points.size(); ++i) {
    edges.emplace_back("line." + std::to_string(i));
    REQUIRE(sketch.add_entity(
        LineSegment::create(edges.back(), points[i], points[(i + 1) % points.size()]).value()));
  }
  REQUIRE(sketch.add_profile(ClosedProfile::create(ProfileId("profile"), edges).value()));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_body(Body::create(BodyId("body"), "Body", BodyKind::Solid).value()));
  auto feature = Feature::create_additive_extrude(FeatureId("base:/odd"), "Base", sketch.id(),
                                                  ParameterId("depth"))
                     .value()
                     .with_profile(ProfileId("profile"))
                     .value()
                     .with_body_result_context(
                         FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody,
                                                          std::nullopt, BodyId("body"))
                             .value())
                     .value();
  REQUIRE(part.add_feature(feature));
  return part;
}
} // namespace

TEST_CASE("Drawn rectangles share semantic roles and face workplanes in their actual source frame",
          "[geometry][finishing-selection][rectangular-finishing]") {
  const auto part = drawn_box();
  auto cache = ShapeCache::create(ShapeCacheId("cache")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto edge =
      SemanticEdgeReference::create(FeatureId("base:/odd"), SemanticEdge::TopFront).value();
  auto resolved = SemanticReferenceEvaluator{}.resolve_edge(part, edge, cache);
  REQUIRE(resolved);
  CHECK(resolved.value().start.x == Catch::Approx(50));
  CHECK(resolved.value().start.y == Catch::Approx(20));
  CHECK(resolved.value().start.z == Catch::Approx(62));
  CHECK(resolved.value().end == Point3{50, 40, 62});
  auto plane = WorkplaneResolver{}.resolve_generated_face(
      part, SemanticFaceReference::create(edge.source_feature(), SemanticFace::Top).value());
  REQUIRE(plane);
  CHECK(plane.value().origin == Point3{50, 30, 56});
  CHECK(plane.value().normal == Vector3{1, 0, 0});
  for (auto role : {PartFeatureInputRole::FilletEdge, PartFeatureInputRole::ChamferEdge,
                    PartFeatureInputRole::ShellRemovalFace, PartFeatureInputRole::DraftFace}) {
    auto targets = FinishingSelectionBuilder::build(part, cache, BodyId("body"), role);
    REQUIRE(targets);
    CHECK(targets.value().size() ==
          (role == PartFeatureInputRole::FilletEdge || role == PartFeatureInputRole::ChamferEdge
               ? 12U
               : 6U));
    std::set<std::string> identities;
    for (const auto& target : targets.value()) {
      CHECK(identities.insert(target.semantic_id).second);
      CHECK_FALSE(target.shape.empty());
    }
  }
  CHECK_FALSE(FinishingSelectionBuilder::build(part, cache, BodyId("missing"),
                                               PartFeatureInputRole::FilletEdge));
  CHECK_FALSE(FinishingSelectionBuilder::build(part, cache, BodyId("body"),
                                               PartFeatureInputRole::MirrorPlane));
}

TEST_CASE("Finishing selection omits edges removed by a later feature on the current Body",
          "[geometry][finishing-selection][rectangular-finishing]") {
  auto part = drawn_box();
  auto reference =
      EdgeReference::create_linear(
          PartFeatureInputRole::FilletEdge,
          SemanticEdgeReference::create(FeatureId("base:/odd"), SemanticEdge::TopFront).value())
          .value();
  REQUIRE(
      part.add_fillet_feature(FilletFeature::create(FeatureId("fillet"), "Fillet", BodyId("body"),
                                                    {reference}, ParameterId("radius"))
                                  .value()));
  auto cache = ShapeCache::create(ShapeCacheId("finished.cache")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  auto targets = FinishingSelectionBuilder::build(part, cache, BodyId("body"),
                                                  PartFeatureInputRole::FilletEdge);
  REQUIRE(targets);
  CHECK_FALSE(targets.value().empty());
  CHECK(targets.value().size() < 12U);
  for (const auto& target : targets.value())
    CHECK_FALSE(target.semantic_id.ends_with("top_front"));
}
