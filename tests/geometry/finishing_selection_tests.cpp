#include "blcad/geometry/result_face_support.hpp"
#include <nlohmann/json.hpp>
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/semantic_reference_evaluator.hpp"
#include "blcad/geometry/workplane_resolver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
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
  CHECK(std::any_of(targets.value().begin(), targets.value().end(), [](const auto& target) {
    return std::get<EdgeReference>(target.reference).source_kind() ==
           PartFeatureInputSourceKind::ResultEdge;
  }));
  for (const auto& target : targets.value())
    CHECK_FALSE(target.semantic_id.ends_with("top_front"));
}

TEST_CASE(
    "Curved current-result rims support fillet then chamfer and fail closed after shape changes",
    "[geometry][finishing-selection][result-edge][curved-finishing]") {
  auto part = PartDocument::create(DocumentId("cylinder"), "Cylinder").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  for (const auto& [id, value] : {std::pair{"diameter", 20.0}, {"depth", 8.0}, {"radius", 1.0}})
    REQUIRE(part.add_parameter(
        Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value).value()).value()));
  auto sketch = Sketch::create(SketchId("circle"), "Circle", DatumPlaneId("datum.xy")).value();
  REQUIRE(sketch.add_profile(
      CircleProfile::create(ProfileId("profile"), ParameterId("diameter")).value()));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_body(Body::create(BodyId("body"), "Body", BodyKind::Solid).value()));
  REQUIRE(part.add_feature(Feature::create_additive_extrude(FeatureId("extrude"), "Cylinder",
                                                            sketch.id(), ParameterId("depth"))
                               .value()
                               .with_profile(ProfileId("profile"))
                               .value()
                               .with_body_result_context(FeatureBodyResultContext::create(
                                                             FeatureBodyOperationMode::NewBody,
                                                             std::nullopt, BodyId("body"))
                                                             .value())
                               .value()));
  auto cache = ShapeCache::create(ShapeCacheId("cylinder.cache")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  SECTION("original cylinder result") {}
  SECTION("translated cylinder result") {
    REQUIRE(part.add_body_transform(
        BodyTransform::create_translate(BodyTransformId("translate"), BodyId("body"),
                                        BodyTransformCoordinateSpace::World, std::nullopt,
                                        {3, 2, 0}, false, false)
            .value()));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  }
  auto targets = FinishingSelectionBuilder::build(part, cache, BodyId("body"),
                                                  PartFeatureInputRole::FilletEdge);
  REQUIRE(targets);
  auto top = std::find_if(targets.value().begin(), targets.value().end(), [](const auto& target) {
    const auto* witness =
        std::get_if<ResultEdgeReference>(&std::get<EdgeReference>(target.reference).source());
    return witness && witness->curve_type() == 1 && target.anchor.z > 7;
  });
  REQUIRE(top != targets.value().end());
  REQUIRE(part.add_fillet_feature(
      FilletFeature::create(FeatureId("fillet"), "Round rim", BodyId("body"),
                            {std::get<EdgeReference>(top->reference)}, ParameterId("radius"))
          .value()));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  targets = FinishingSelectionBuilder::build(part, cache, BodyId("body"),
                                             PartFeatureInputRole::ChamferEdge);
  REQUIRE(targets);
  auto bottom =
      std::find_if(targets.value().begin(), targets.value().end(), [](const auto& target) {
        const auto* witness =
            std::get_if<ResultEdgeReference>(&std::get<EdgeReference>(target.reference).source());
        return witness && witness->curve_type() == 1 && target.anchor.z < 0.1;
      });
  REQUIRE(bottom != targets.value().end());
  CHECK(std::get<EdgeReference>(bottom->reference).source_node_id() == "fillet");
  REQUIRE(part.add_chamfer_feature(
      ChamferFeature::create_equal_distance(FeatureId("chamfer"), "Bevel rim", BodyId("body"),
                                            {std::get<EdgeReference>(bottom->reference)},
                                            ParameterId("radius"))
          .value()));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto volume =
      RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body"))).volume_mm3;
  CHECK(volume < 800 * std::acos(-1.0));
  CHECK(volume > 700 * std::acos(-1.0));
  double final_volume = volume;
  SECTION("finished result alone") {}
  SECTION("Union consumes the finished result") {
    REQUIRE(part.add_body(Body::create(BodyId("tool"), "Union tool", BodyKind::Solid).value()));
    REQUIRE(part.add_feature(
        Feature::create_additive_extrude(FeatureId("tool.extrude"), "Tool cylinder", sketch.id(),
                                         ParameterId("depth"))
            .value()
            .with_profile(ProfileId("profile"))
            .value()
            .with_body_result_context(
                FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                 BodyId("tool"))
                    .value())
            .value()));
    REQUIRE(part.add_body_boolean_feature(
        BodyBooleanFeature::create(FeatureId("union"), BodyBooleanOperation::Add, BodyId("body"),
                                   {BodyId("tool")}, BodyBooleanResultMode::ModifyTarget,
                                   std::nullopt, false)
            .value()));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
    const auto summary =
        RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body")));
    CHECK(summary.solid_count == 1);
    CHECK(summary.volume_mm3 > volume);
    final_volume = summary.volume_mm3;
  }
  const auto json = serialize_part_document_to_json(part).value();
  auto restored = deserialize_part_document_from_json(json);
  REQUIRE(restored);
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(restored.value(), cache));
  CHECK(RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body"))).volume_mm3 ==
        Catch::Approx(final_volume));
  REQUIRE(restored.value().set_parameter_value(ParameterId("diameter"),
                                               Quantity::length_mm(22).value()));
  auto changed = GeometryRecomputeExecutor{}.execute_document(restored.value(), cache);
  REQUIRE_FALSE(changed);
  CHECK(changed.error().message().find("select the edge again") != std::string::npos);
}

namespace {
ResultFaceWorkplane face_support(const PartDocument& part, const ShapeCache& cache,
                                 const std::string& role = "extrude/upper-cap") {
  auto targets = ResultFaceSupportGeometry::selectable_faces(part, cache, BodyId("body"));
  REQUIRE(targets);
  const auto found =
      std::find_if(targets.value().begin(), targets.value().end(), [&](const auto& target) {
        return std::any_of(target.reference.origins().begin(), target.reference.origins().end(),
                           [&](const auto& origin) { return origin.role == role; });
      });
  REQUIRE(found != targets.value().end());
  return ResultFaceWorkplane::create(DatumPlaneId("support"), "Face Workplane 1", found->reference)
      .value();
}
void require_near(Point3 actual, Point3 expected) {
  CHECK(actual.x == Catch::Approx(expected.x).margin(1e-7));
  CHECK(actual.y == Catch::Approx(expected.y).margin(1e-7));
  CHECK(actual.z == Catch::Approx(expected.z).margin(1e-7));
}
} // namespace

TEST_CASE(
    "Result-face support follows thickness with a dependent blind pocket and strict roundtrip",
    "[geometry][result-face-support][result-face-pocket]") {
  auto part = drawn_box();
  REQUIRE(part.set_parameter_value(ParameterId("depth"), Quantity::length_mm(8).value()));
  auto cache = ShapeCache::create(ShapeCacheId("face.cache")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto plane = face_support(part, cache);
  auto sketch = Sketch::create(SketchId("pocket.sketch"), "Pocket", plane.id()).value();
  REQUIRE(part.add_parameter(Parameter::create_length(ParameterId("pocket.width"), "Pocket width",
                                                      Quantity::length_mm(4).value())
                                 .value()));
  REQUIRE(part.add_parameter(Parameter::create_length(ParameterId("pocket.depth"), "Pocket depth",
                                                      Quantity::length_mm(2).value())
                                 .value()));
  REQUIRE(sketch.add_profile(RectangleProfile::create(ProfileId("pocket.profile"),
                                                      ParameterId("pocket.width"),
                                                      ParameterId("pocket.width"), {-20, -14})
                                 .value()));
  REQUIRE(part.add_face_supported_sketch(plane, sketch));
  CHECK(part.dependency_graph().has_dependency(plane.id().value(), sketch.id().value()));
  auto extent = ExtrudeFeatureIntent::create(
                    ExtrudeExtentIntent::distance(ParameterId("pocket.depth")).value())
                    .value();
  auto pocket = Feature::create_subtractive_extrude(FeatureId("pocket"), "Pocket", sketch.id(),
                                                    FeatureId("base:/odd"), extent,
                                                    ExtrudeDirection::OppositeSketchNormal)
                    .value();
  pocket =
      pocket.with_profile(ProfileId("pocket.profile"))
          .value()
          .with_body_result_context(FeatureBodyResultContext::create(FeatureBodyOperationMode::Cut,
                                                                     BodyId("body"), std::nullopt)
                                        .value())
          .value();
  REQUIRE(part.add_feature(pocket));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  CHECK(RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body"))).volume_mm3 ==
        Catch::Approx(20 * 12 * 8 - 4 * 4 * 2));
  auto frame = ResultFaceSupportGeometry::resolve(part, plane, cache);
  REQUIRE(frame);
  require_near(frame.value().origin, {48, 50, 70});
  CHECK(frame.value().x_axis == Vector3{0, 1, 0});
  CHECK(frame.value().y_axis == Vector3{0, 0, 1});
  part.mark_all_clean();
  REQUIRE(part.set_parameter_value(ParameterId("depth"), Quantity::length_mm(20).value()));
  const auto plan = part.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(GeometryRecomputeExecutor{}.execute_plan(part, plan.value(), cache));
  frame = ResultFaceSupportGeometry::resolve(part, plane, cache);
  REQUIRE(frame);
  require_near(frame.value().origin, {60, 50, 70});
  CHECK(part.find_sketch(sketch.id())->rectangle_profiles().front().center() == Point2{-20, -14});
  const auto actual =
      RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body"))).volume_mm3;
  CHECK(actual == Catch::Approx(20 * 12 * 20 - 4 * 4 * 2));
  const auto json = serialize_part_document_to_json(part);
  REQUIRE(json);
  auto reopened = deserialize_part_document_from_json(json.value());
  REQUIRE(reopened);
  CHECK(serialize_part_document_to_json(reopened.value()).value() == json.value());
  auto full = ShapeCache::create(ShapeCacheId("face.full")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(reopened.value(), full));
  CHECK(RectangleExtrusionAdapter{}.summarize(*full.find_body_shape(BodyId("body"))).volume_mm3 ==
        Catch::Approx(actual));
  CHECK_FALSE(part.update_result_face_workplane(
      ResultFaceWorkplane::create(plane.id(), plane.name(),
                                  ResultFaceReference::create(FeatureId("pocket"), BodyId("body"),
                                                              plane.reference().origins())
                                      .value())
          .value()));
  CHECK(serialize_part_document_to_json(part).value() == json.value());
  auto malformed = nlohmann::json::parse(json.value());
  malformed["result_face_workplanes"][0]["reference"]["occt_index"] = 1;
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  malformed = nlohmann::json::parse(json.value());
  malformed["result_face_workplanes"][0]["reference"]["origins"][0]["role"] = nullptr;
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  malformed = nlohmann::json::parse(json.value());
  malformed["feature_order"] = nlohmann::json::array({"pocket", "base:/odd"});
  CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  auto absent = nlohmann::json::parse(json.value());
  absent["features"].erase(absent["features"].begin());
  CHECK_FALSE(deserialize_part_document_from_json(absent.dump()));
}

TEST_CASE("Result-face creation rejects a mismatched Sketch atomically and preserves legacy bytes",
          "[geometry][result-face-support][result-face-atomic]") {
  auto part = drawn_box();
  auto cache = ShapeCache::create(ShapeCacheId("atomic")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto before = serialize_part_document_to_json(part).value();
  CHECK(before.find("result_face_workplanes") == std::string::npos);
  const auto plane = face_support(part, cache);
  auto sketch = Sketch::create(SketchId("wrong"), "Wrong", DatumPlaneId("plane")).value();
  CHECK_FALSE(part.add_face_supported_sketch(plane, sketch));
  CHECK(serialize_part_document_to_json(part).value() == before);
  CHECK(part.result_face_workplane_count() == 0);
  CHECK(serialize_part_document_to_json(deserialize_part_document_from_json(before).value())
            .value() == before);
}

TEST_CASE("Authored face lineage follows Body rotation without a second reference transform",
          "[geometry][result-face-support][result-face-transform]") {
  auto part = drawn_box();
  const auto axis = BodyTransformRotationAxis::create_explicit({0, 0, 0}, {0, 0, 1}).value();
  REQUIRE(part.add_body_transform(BodyTransform::create_rotate(BodyTransformId("rotation"),
                                                               BodyId("body"),
                                                               BodyTransformCoordinateSpace::World,
                                                               std::nullopt, axis, 90, false, false)
                                      .value()));
  auto cache = ShapeCache::create(ShapeCacheId("rotated")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto plane = face_support(part, cache);
  REQUIRE(plane.reference().producer() == FeatureId("rotation"));
  REQUIRE(part.add_face_supported_sketch(
      plane, Sketch::create(SketchId("attached"), "Attached", plane.id()).value()));
  auto frame =
      WorkplaneResolver{}.resolve_for_sketch(part, *part.find_sketch(SketchId("attached")), cache);
  REQUIRE(frame);
  require_near(frame.value().origin, {-50, 50, 70});
  CHECK(frame.value().normal.x == Catch::Approx(0).margin(1e-7));
  CHECK(frame.value().normal.y == Catch::Approx(1));
  auto edited = nlohmann::json::parse(serialize_part_document_to_json(part).value());
  edited["body_transforms"][0]["angle_deg"] = 180;
  auto changed = deserialize_part_document_from_json(edited.dump());
  REQUIRE(changed);
  part = std::move(changed.value());
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  frame =
      WorkplaneResolver{}.resolve_for_sketch(part, *part.find_sketch(SketchId("attached")), cache);
  REQUIRE(frame);
  require_near(frame.value().origin, {-50, -50, 70});
  CHECK(frame.value().normal.x == Catch::Approx(-1));
  const auto json = serialize_part_document_to_json(part).value();
  auto reopened = deserialize_part_document_from_json(json);
  REQUIRE(reopened);
  CHECK(serialize_part_document_to_json(reopened.value()).value() == json);
}

TEST_CASE("Boolean result face splits and deletion fail closed while unchanged lineage remains "
          "selectable",
          "[geometry][result-face-support][result-face-recovery]") {
  auto part = drawn_box();
  REQUIRE(part.set_parameter_value(ParameterId("depth"), Quantity::length_mm(8).value()));
  REQUIRE(part.add_construction_plane(
      ConstructionPlane::create_explicit(ConstructionPlaneId("tool.plane"), "Tool plane",
                                         {46, 50, 70}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0})
          .value()));
  for (const auto& [id, value] :
       {std::pair{"tool.width", 2.0}, {"tool.height", 4.0}, {"tool.depth", 10.0}})
    REQUIRE(part.add_parameter(
        Parameter::create_length(ParameterId(id), id, Quantity::length_mm(value).value()).value()));
  auto sketch = Sketch::create(SketchId("tool.sketch"), "Tool", DatumPlaneId("tool.plane")).value();
  REQUIRE(sketch.add_profile(RectangleProfile::create(ProfileId("tool.profile"),
                                                      ParameterId("tool.width"),
                                                      ParameterId("tool.height"), {-20, -14})
                                 .value()));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_body(Body::create(BodyId("tool"), "Tool", BodyKind::Solid).value()));
  auto tool = Feature::create_additive_extrude(FeatureId("tool"), "Tool", sketch.id(),
                                               ParameterId("tool.depth"))
                  .value();
  tool = tool.with_body_result_context(
                 FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId("tool"))
                     .value())
             .value();
  REQUIRE(part.add_feature(tool));
  REQUIRE(part.add_body_boolean_feature(
      BodyBooleanFeature::create(FeatureId("cut"), BodyBooleanOperation::Subtract, BodyId("body"),
                                 {BodyId("tool")}, BodyBooleanResultMode::ModifyTarget,
                                 std::nullopt, true)
          .value()));
  auto cache = ShapeCache::create(ShapeCacheId("boolean")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto plane = face_support(part, cache);
  CHECK(plane.reference().producer() == FeatureId("cut"));
  SECTION("split") {
    REQUIRE(part.set_parameter_value(ParameterId("tool.height"), Quantity::length_mm(30).value()));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
    const auto recovered = ResultFaceSupportGeometry::resolve(part, plane, cache);
    REQUIRE_FALSE(recovered);
    CHECK(recovered.error().message().find("Ambiguous") != std::string::npos);
    auto offered = ResultFaceSupportGeometry::selectable_faces(part, cache, BodyId("body"));
    REQUIRE(offered);
    CHECK(std::none_of(offered.value().begin(), offered.value().end(), [&](const auto& candidate) {
      return candidate.reference == plane.reference();
    }));
  }
  SECTION("deleted face") {
    REQUIRE(part.set_parameter_value(ParameterId("tool.width"), Quantity::length_mm(50).value()));
    REQUIRE(part.set_parameter_value(ParameterId("tool.height"), Quantity::length_mm(50).value()));
    REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
    const auto recovered = ResultFaceSupportGeometry::resolve(part, plane, cache);
    REQUIRE_FALSE(recovered);
    CHECK(recovered.error().message().find("Lost") != std::string::npos);
  }
}

TEST_CASE("Finishing preserves planar face origins and rejects curved fillet supports",
          "[geometry][result-face-support][result-face-finishing]") {
  auto part = drawn_box();
  auto edge =
      EdgeReference::create_linear(
          PartFeatureInputRole::FilletEdge,
          SemanticEdgeReference::create(FeatureId("base:/odd"), SemanticEdge::TopFront).value())
          .value();
  REQUIRE(
      part.add_fillet_feature(FilletFeature::create(FeatureId("fillet"), "Fillet", BodyId("body"),
                                                    {edge}, ParameterId("radius"))
                                  .value()));
  auto cache = ShapeCache::create(ShapeCacheId("finished.support")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto plane = face_support(part, cache);
  CHECK(plane.reference().producer() == FeatureId("fillet"));
  REQUIRE(part.add_face_supported_sketch(
      plane, Sketch::create(SketchId("attached"), "Attached", plane.id()).value()));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  CHECK(ResultFaceSupportGeometry::resolve(part, plane, cache));
}

TEST_CASE("Oblique polygon support follows profile and datum edits without rewriting local Sketch "
          "coordinates",
          "[geometry][result-face-support][result-face-oblique]") {
  auto part = drawn_box();
  const double s = std::sqrt(0.5);
  REQUIRE(part.update_construction_plane(
      ConstructionPlane::create_explicit(ConstructionPlaneId("plane"), "Oblique", {12, 23, 34},
                                         {s, s, 0}, {0, 0, 1}, {s, -s, 0})
          .value()));
  const auto polygon = [&](double width) {
    auto sketch = Sketch::create(SketchId("rectangle"), "Polygon", DatumPlaneId("plane")).value();
    const std::vector<Point2> points{{0, 0}, {width, 0}, {width + 2, 5}, {width / 2, 12}, {-2, 6}};
    std::vector<SketchEntityId> ids;
    for (std::size_t i = 0; i < points.size(); ++i) {
      ids.emplace_back("edge." + std::to_string(i));
      REQUIRE(sketch.add_entity(
          LineSegment::create(ids.back(), points[i], points[(i + 1) % points.size()]).value()));
    }
    REQUIRE(sketch.add_profile(ClosedProfile::create(ProfileId("profile"), ids).value()));
    return sketch;
  };
  REQUIRE(part.update_sketch(polygon(10)));
  auto cache = ShapeCache::create(ShapeCacheId("polygon")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto plane = face_support(part, cache);
  auto sketch = Sketch::create(SketchId("attached"), "Attached", plane.id()).value();
  REQUIRE(sketch.add_entity(LineSegment::create(SketchEntityId("local"), {1, 2}, {3, 4}).value()));
  REQUIRE(part.add_face_supported_sketch(plane, sketch));
  auto frame = ResultFaceSupportGeometry::resolve(part, plane, cache);
  REQUIRE(frame);
  require_near(frame.value().origin, {12 + 10 * s, 23 - 10 * s, 34});
  CHECK(frame.value().x_axis.x == Catch::Approx(s));
  CHECK(frame.value().x_axis.y == Catch::Approx(s));
  part.mark_all_clean();
  REQUIRE(part.update_sketch(polygon(16)));
  REQUIRE(part.update_construction_plane(
      ConstructionPlane::create_explicit(ConstructionPlaneId("plane"), "Moved oblique",
                                         {22, 33, 44}, {s, s, 0}, {0, 0, 1}, {s, -s, 0})
          .value()));
  auto plan = part.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(GeometryRecomputeExecutor{}.execute_plan(part, plan.value(), cache));
  frame = ResultFaceSupportGeometry::resolve(part, plane, cache);
  REQUIRE(frame);
  require_near(frame.value().origin, {22 + 10 * s, 33 - 10 * s, 44});
  const auto* local = part.find_sketch(sketch.id())->find_line_segment(SketchEntityId("local"));
  REQUIRE(local);
  CHECK(local->start() == Point2{1, 2});
  CHECK(local->end() == Point2{3, 4});
  CHECK(part.find_result_face_workplane(plane.id())->reference() == plane.reference());
}

TEST_CASE("Missing result-face sources load only for explicit reference repair and can be rebound",
          "[geometry][result-face-support][result-face-repair]") {
  auto part = drawn_box();
  auto cache = ShapeCache::create(ShapeCacheId("repair")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto valid = face_support(part, cache);
  REQUIRE(part.add_face_supported_sketch(
      valid, Sketch::create(SketchId("attached"), "Attached", valid.id()).value()));
  auto root = nlohmann::json::parse(serialize_part_document_to_json(part).value());
  auto& reference = root["result_face_workplanes"][0]["reference"];
  reference["producer"] = "deleted";
  reference["origins"][0]["feature"] = "deleted";
  CHECK_FALSE(deserialize_part_document_from_json(root.dump()));
  auto repaired =
      deserialize_part_document_from_json(root.dump(), PartDocumentReadMode::ReferenceRepair);
  REQUIRE(repaired);
  auto failed = GeometryRecomputeExecutor{}.execute_document(repaired.value(), cache);
  CHECK_FALSE(failed);
  REQUIRE(repaired.value().update_result_face_workplane(valid));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(repaired.value(), cache));
  CHECK(ResultFaceSupportGeometry::resolve(repaired.value(), valid, cache));
  CHECK(repaired.value().find_sketch(SketchId("attached"))->workplane() == valid.id());
}

TEST_CASE("Curved cylinder walls cannot supply a planar Sketch support",
          "[geometry][result-face-support][result-face-curved]") {
  auto part = drawn_box();
  REQUIRE(part.add_parameter(
      Parameter::create_length(ParameterId("diameter"), "Diameter", Quantity::length_mm(20).value())
          .value()));
  auto sketch = Sketch::create(SketchId("rectangle"), "Circle", DatumPlaneId("plane")).value();
  REQUIRE(sketch.add_profile(
      CircleProfile::create(ProfileId("profile"), ParameterId("diameter")).value()));
  REQUIRE(part.update_sketch(sketch));
  auto cache = ShapeCache::create(ShapeCacheId("curved.support")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  CHECK(RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body"))).volume_mm3 ==
        Catch::Approx(100 * std::acos(-1.0) * 10));
  auto targets = ResultFaceSupportGeometry::selectable_faces(part, cache, BodyId("body"));
  REQUIRE(targets);
  REQUIRE(targets.value().size() == 2);
  for (const auto& target : targets.value()) {
    REQUIRE(target.reference.origins().size() == 1);
    CHECK((target.reference.origins().front().role == "extrude/lower-cap" ||
           target.reference.origins().front().role == "extrude/upper-cap"));
  }
}

TEST_CASE("Empty Sketches on lost support fail full and incremental recompute without publishing "
          "partial caches",
          "[geometry][result-face-support][result-face-freshness]") {
  auto part = drawn_box();
  auto cache = ShapeCache::create(ShapeCacheId("lost.empty")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto reference =
      ResultFaceReference::create(FeatureId("base:/odd"), BodyId("body"),
                                  {{FeatureId("base:/odd"), "deleted-authored-role"}})
          .value();
  const auto plane =
      ResultFaceWorkplane::create(DatumPlaneId("lost"), "Lost face", reference).value();
  REQUIRE(part.add_face_supported_sketch(
      plane, Sketch::create(SketchId("empty"), "Empty", plane.id()).value()));
  const auto volume =
      RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body"))).volume_mm3;
  const auto count = cache.feature_shape_count();
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  CHECK(cache.feature_shape_count() == count);
  CHECK(RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body"))).volume_mm3 ==
        Catch::Approx(volume));
  REQUIRE(part.set_parameter_value(ParameterId("depth"), Quantity::length_mm(12).value()));
  auto plan = part.create_recompute_plan();
  REQUIRE(plan);
  REQUIRE(std::any_of(plan.value().steps().begin(), plan.value().steps().end(),
                      [&](const auto& step) { return step.node_id == plane.id().value(); }));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_plan(part, plan.value(), cache));
  CHECK(cache.feature_shape_count() == count);
  CHECK(RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body"))).volume_mm3 ==
        Catch::Approx(volume));
}

TEST_CASE("Owned face-supported Sketches apply only the Body pose after their captured producer",
          "[geometry][result-face-support][result-face-owned-pose]") {
  auto part = drawn_box();
  REQUIRE(part.add_body_transform(
      BodyTransform::create_translate(BodyTransformId("before"), BodyId("body"),
                                      BodyTransformCoordinateSpace::World, std::nullopt, {2, 3, 4},
                                      true, true)
          .value()));
  auto cache = ShapeCache::create(ShapeCacheId("owned.support")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto plane = face_support(part, cache);
  REQUIRE(part.add_face_supported_sketch(
      plane, Sketch::create(SketchId("attached"), "Attached", plane.id()).value()));
  REQUIRE(part.add_sketch_ownership(SketchOwnership::create(SketchId("attached"), BodyId("body"),
                                                            SketchAssociation::ReferenceOnly)
                                        .value()));
  REQUIRE(part.add_body_transform(
      BodyTransform::create_translate(BodyTransformId("after"), BodyId("body"),
                                      BodyTransformCoordinateSpace::World, std::nullopt, {5, 6, 7},
                                      true, true)
          .value()));
  auto full = ShapeCache::create(ShapeCacheId("owned.full")).value();
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(part, full));
  const auto frame =
      WorkplaneResolver{}.resolve_for_sketch(part, *part.find_sketch(SketchId("attached")), full);
  REQUIRE(frame);
  require_near(frame.value().origin, {57, 59, 81});
}
