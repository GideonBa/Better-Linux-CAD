#include "../support/thread_feature_fixture.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/thread_adapter.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>
#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::thread_fixture;
using f::checked;

namespace {
ShapeCache evaluate(const PartDocument& part) {
  auto cache = checked(ShapeCache::create(ShapeCacheId("thread.result")));
  auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  INFO((result ? "ok" : result.error().object_id() + ": " + result.error().message()));
  REQUIRE(result);
  REQUIRE(cache.validate_complete_geometry_result(part));
  return cache;
}
double volume(const GeometryShape& shape) {
  GProp_GProps props;
  BRepGProp::VolumeProperties(*OcctShapeView::get(shape), props, 1e-9);
  return props.Mass();
}
int topology(const GeometryShape& shape, TopAbs_ShapeEnum kind) {
  TopTools_IndexedMapOfShape entities;
  TopExp::MapShapes(*OcctShapeView::get(shape), kind, entities);
  return entities.Extent();
}
double largest_tolerance(const GeometryShape& shape) {
  double result = 0;
  for (TopExp_Explorer it(*OcctShapeView::get(shape), TopAbs_EDGE); it.More(); it.Next())
    result = std::max(result, BRep_Tool::Tolerance(TopoDS::Edge(it.Current())));
  for (TopExp_Explorer it(*OcctShapeView::get(shape), TopAbs_VERTEX); it.More(); it.Next())
    result = std::max(result, BRep_Tool::Tolerance(TopoDS::Vertex(it.Current())));
  return result;
}

} // namespace

TEST_CASE("Cosmetic Threads preserve the exact shaft and plain pilot geometry",
          "[geometry][parity-b155][thread-cosmetic]") {
  auto part = f::pair_part();
  const auto thread = f::internal();
  auto plain = part;
  REQUIRE(plain.add_hole_feature(checked(HoleFeature::create(
      thread.id(), thread.name(), thread.kind(), thread.target_body(), thread.support(),
      thread.placements(), thread.diameter(), thread.extent(), thread.direction(), thread.tip()))));
  const auto baseline = evaluate(plain);
  REQUIRE(part.add_hole_feature(thread));
  REQUIRE(part.add_thread_feature(f::external()));
  auto result = evaluate(part);
  REQUIRE(OcctShapeView::get(*result.find_feature_shape(FeatureId("shaft")))
              ->IsSame(*OcctShapeView::get(*result.find_feature_shape(FeatureId("external")))));
  const auto& pilot = *result.find_body_shape(BodyId("body.base"));
  const auto& original = *baseline.find_body_shape(BodyId("body.base"));
  CHECK(volume(pilot) == Catch::Approx(volume(original)).margin(1e-7));
  CHECK(topology(pilot, TopAbs_FACE) == topology(original, TopAbs_FACE));
  CHECK(topology(pilot, TopAbs_EDGE) == topology(original, TopAbs_EDGE));
  CHECK(topology(pilot, TopAbs_VERTEX) == topology(original, TopAbs_VERTEX));
  for (const auto& body : {BodyId("body.base"), BodyId("body.shaft")}) {
    const auto symbols =
        checked(ThreadAdapter::cosmetic_presentations(*result.find_body_shape(body)));
    REQUIRE(symbols.size() == 1);
    CHECK(topology(symbols.front().shape, TopAbs_SOLID) == 0);
    CHECK(topology(symbols.front().shape, TopAbs_FACE) == 0);
    CHECK(topology(symbols.front().shape, TopAbs_EDGE) >= 2);
  }
  const auto scene = checked(ViewportSceneBuilder{}.build_part(part, result));
  CHECK(std::count_if(scene.begin(), scene.end(), [](const auto& item) {
          return item.kind == ViewportSceneKind::ThreadAnnotation;
        }) == 2);
  auto reopened =
      checked(deserialize_part_document_from_json(checked(serialize_part_document_to_json(part))));
  auto fresh = evaluate(reopened);
  CHECK(volume(*fresh.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(volume(pilot)).margin(1e-7));
}

TEST_CASE(
    "Modeled internal and external Threads have independent pitch hand profile and volume oracles",
    "[geometry][parity-b155][thread-modeled]") {
  for (double pitch : {1.7, 2.2})
    for (auto hand : {ThreadHandedness::Right, ThreadHandedness::Left}) {
      CAPTURE(pitch, hand == ThreadHandedness::Right);
      auto part = f::pair_part();
      REQUIRE(part.set_parameter_value(ParameterId("thread.pitch"),
                                       checked(Quantity::length_mm(pitch))));
      const auto definition = f::definition(f::custom(), true, hand, ThreadRepresentation::Modeled);
      REQUIRE(part.add_hole_feature(f::internal(definition)));
      REQUIRE(part.add_thread_feature(f::external(definition)));
      const auto result = evaluate(part);
      const double flank = std::tan(55 * std::numbers::pi / 360);
      for (bool internal : {false, true}) {
        const auto& shape = *result.find_body_shape(BodyId(internal ? "body.base" : "body.shaft"));
        INFO("Largest boundary tolerance: " << largest_tolerance(shape));
        REQUIRE(BRepCheck_Analyzer(*OcctShapeView::get(shape)).IsValid());
        CHECK(topology(shape, TopAbs_SOLID) == 1);
        CHECK(largest_tolerance(shape) <= 1e-4);
        const double lost = 4 * std::numbers::pi * flank * 4 / pitch *
                            (internal ? (5. / 2 - 1. / 3) : (4. / 2 + 1. / 3));
        const double baseline =
            internal ? 2560 - std::numbers::pi * 16 * 6 : 200 * std::numbers::pi;
        CHECK(volume(shape) == Catch::Approx(baseline - lost).margin(2e-4));
        std::size_t cut_samples = 0, solid_samples = 0;
        for (double z : {.35, 1.05, 1.85, 2.7, 3.4})
          for (double theta : {.23, 1.17, 2.39, 3.65, 5.31})
            for (double r : {4.25, 4.75}) {
              const double phase = std::remainder(z - (hand == ThreadHandedness::Right ? 1. : -1.) *
                                                          pitch * theta / (2 * std::numbers::pi),
                                                  pitch);
              const double width = (internal ? 5 - r : r - 4) * flank;
              if (std::abs(std::abs(phase) - width) < .025)
                continue;
              const bool removed = std::abs(phase) < width;
              CAPTURE(internal, z, theta, r, phase, width);
              const gp_Pnt p(internal ? r * std::cos(theta) : 30 + r * std::cos(theta),
                             (internal ? -1 : 1) * r * std::sin(theta),
                             internal ? 8 - 1 - z : 1 + z);
              BRepClass3d_SolidClassifier classifier(*OcctShapeView::get(shape), p, 1e-7);
              CHECK(classifier.State() == (removed ? TopAbs_OUT : TopAbs_IN));
              removed ? ++cut_samples : ++solid_samples;
            }
        CHECK(cut_samples > 5);
        CHECK(solid_samples > 5);
        // Unthreaded lead and blind bottom stay unchanged, independently of hand.
        const gp_Pnt lead(internal ? 4.5 : 34.5, 0, internal ? 7.5 : .5);
        CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(shape), lead, 1e-7).State() ==
              TopAbs_IN);
        if (internal)
          CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(shape), gp_Pnt(4.5, 0, 2.5), 1e-7)
                    .State() == TopAbs_IN);
      }
    }
}

TEST_CASE("Invalid external Thread support cannot replace any previously computed Body",
          "[geometry][parity-b155][thread-boundaries]") {
  const auto original = f::pair_part();
  auto cache = checked(ShapeCache::create(ShapeCacheId("thread.atomic")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(original, cache));
  const auto base = *cache.find_body_shape(BodyId("body.base"));
  const auto shaft = *cache.find_body_shape(BodyId("body.shaft"));
  auto part = original;
  const auto support = blcad::test::history_fixture::face(PartFeatureInputRole::ThreadSupportFace);
  REQUIRE(part.add_thread_feature(
      checked(ThreadFeature::create(FeatureId("invalid.thread"), "Invalid planar support",
                                    BodyId("body.base"), support, f::definition()))));
  const auto intent = checked(serialize_part_document_to_json(part));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  REQUIRE(cache.validate_complete_geometry_result(original));
  CHECK(OcctShapeView::get(base)->IsSame(
      *OcctShapeView::get(*cache.find_body_shape(BodyId("body.base")))));
  CHECK(OcctShapeView::get(shaft)->IsSame(
      *OcctShapeView::get(*cache.find_body_shape(BodyId("body.shaft")))));
  CHECK(checked(serialize_part_document_to_json(part)) == intent);
}

TEST_CASE("An outward blind Threaded Hole cannot silently drop metadata or replace cached Bodies",
          "[geometry][parity-b155][thread-boundaries]") {
  const auto original = f::pair_part();
  auto cache = checked(ShapeCache::create(ShapeCacheId("thread.hole.atomic")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(original, cache));
  const auto base = *cache.find_body_shape(BodyId("body.base"));
  auto part = original;
  const auto hole = f::internal();
  REQUIRE(part.add_hole_feature(checked(HoleFeature::create(
      hole.id(), hole.name(), hole.kind(), hole.target_body(), hole.support(), hole.placements(),
      hole.diameter(), hole.extent(), HoleDirection::SupportNormal, hole.tip(),
      hole.manufacturing(), hole.thread()))));
  const auto intent = checked(serialize_part_document_to_json(part));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(part, cache));
  REQUIRE(cache.validate_complete_geometry_result(original));
  CHECK(OcctShapeView::get(base)->IsSame(
      *OcctShapeView::get(*cache.find_body_shape(BodyId("body.base")))));
  CHECK(checked(serialize_part_document_to_json(part)) == intent);
}

TEST_CASE("External Thread support inspection has independent axis radius and interval oracles",
          "[geometry][parity-b157][thread-support-query]") {
  const auto part = f::pair_part();
  const auto before = checked(serialize_part_document_to_json(part));
  const auto cache = evaluate(part);
  const auto support = f::external().support();
  for (const bool reverse : {false, true}) {
    auto result = ThreadAdapter::inspect_support(part, BodyId("body.shaft"), support, cache,
                                                 reverse ? ThreadDirection::OppositeSupportAxis
                                                         : ThreadDirection::AlongSupportAxis);
    INFO((result ? "ok" : result.error().message()));
    REQUIRE(result);
    const auto& value = result.value();
    CHECK(value.origin.x == Catch::Approx(30).margin(1e-7));
    CHECK(value.origin.y == Catch::Approx(0).margin(1e-7));
    CHECK(value.origin.z == Catch::Approx(reverse ? 8 : 0).margin(1e-7));
    CHECK(value.axis.x == Catch::Approx(0).margin(1e-7));
    CHECK(value.axis.y == Catch::Approx(0).margin(1e-7));
    CHECK(value.axis.z == Catch::Approx(reverse ? -1 : 1).margin(1e-7));
    CHECK(value.radius_mm == Catch::Approx(5).margin(1e-7));
    CHECK(value.axial_length_mm == Catch::Approx(8).margin(1e-7));
    CHECK(value.taper_angle_deg == Catch::Approx(0).margin(1e-7));
    CHECK(dot(value.axis, value.x_axis) == Catch::Approx(0).margin(1e-7));
    CHECK(length(value.x_axis) == Catch::Approx(1).margin(1e-7));
  }
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  CHECK(volume(*cache.find_body_shape(BodyId("body.shaft"))) ==
        Catch::Approx(200 * std::numbers::pi).margin(1e-6));
}

TEST_CASE("External Thread inspection follows actual rigid Body placement and direction",
          "[geometry][parity-b157][thread-support-query]") {
  auto part = f::pair_part();
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("move"), BodyId("body.shaft"), BodyTransformCoordinateSpace::World, {},
      {7, -2, 3}, false, false))));
  auto axis = checked(BodyTransformRotationAxis::create_explicit({}, {1, 0, 0}));
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_rotate(
      BodyTransformId("turn"), BodyId("body.shaft"), BodyTransformCoordinateSpace::World, {}, axis,
      90, false, false))));
  const auto cache = evaluate(part);
  for (const bool reverse : {false, true}) {
    const auto value = checked(ThreadAdapter::inspect_support(
        part, BodyId("body.shaft"), f::external().support(), cache,
        reverse ? ThreadDirection::OppositeSupportAxis : ThreadDirection::AlongSupportAxis));
    CHECK(value.origin.x == Catch::Approx(37).margin(1e-7));
    CHECK(value.origin.y == Catch::Approx(reverse ? -11 : -3).margin(1e-7));
    CHECK(value.origin.z == Catch::Approx(-2).margin(1e-7));
    CHECK(value.axis.x == Catch::Approx(0).margin(1e-7));
    CHECK(value.axis.y == Catch::Approx(reverse ? 1 : -1).margin(1e-7));
    CHECK(value.axis.z == Catch::Approx(0).margin(1e-7));
    CHECK(value.radius_mm == Catch::Approx(5).margin(1e-7));
    CHECK(value.axial_length_mm == Catch::Approx(8).margin(1e-7));
  }
}

TEST_CASE("External Thread inspection rejects planar wrong-body internal and stale supports",
          "[geometry][parity-b157][thread-support-query]") {
  auto part = f::pair_part();
  REQUIRE(part.add_hole_feature(f::internal()));
  const auto cache = evaluate(part);
  const auto top = checked(FaceReference::create_planar(
      PartFeatureInputRole::ThreadSupportFace,
      checked(SemanticFaceReference::create(FeatureId("shaft"), SemanticFace::Top))));
  CHECK_FALSE(ThreadAdapter::inspect_support(part, BodyId("body.shaft"), top, cache));
  CHECK_FALSE(
      ThreadAdapter::inspect_support(part, BodyId("body.base"), f::external().support(), cache));
  const auto wall = checked(FaceReference::create_result(
      PartFeatureInputRole::ThreadSupportFace,
      checked(ResultFaceReference::create(FeatureId("internal"), BodyId("body.base"),
                                          {{FeatureId("internal"), "hole/6:center/wall"}}))));
  CHECK_FALSE(ThreadAdapter::inspect_support(part, BodyId("body.base"), wall, cache));
  const auto before = checked(serialize_part_document_to_json(part));
  auto valid =
      ThreadAdapter::inspect_support(part, BodyId("body.shaft"), f::external().support(), cache);
  REQUIRE(valid);
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  REQUIRE(part.set_parameter_value(ParameterId("depth"), checked(Quantity::length_mm(9))));
  CHECK_FALSE(
      ThreadAdapter::inspect_support(part, BodyId("body.shaft"), f::external().support(), cache));
}

TEST_CASE("Thread support inspection requires an explicit matching pre-feature history scope",
          "[geometry][parity-b157][thread-support-query][thread-support-prefix]") {
  auto part = f::pair_part();
  REQUIRE(part.add_thread_feature(f::external()));
  const auto before = checked(serialize_part_document_to_json(part));
  const auto history = checked(part.executable_history());
  const auto row = std::find_if(history.begin(), history.end(),
                                [](const auto& item) { return item.node_id == "external"; });
  REQUIRE(row != history.end());
  const auto prefix = std::size_t(row - history.begin());
  auto cache = checked(ShapeCache::create(ShapeCacheId("thread.input.scope")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_history_prefix(part, prefix, cache));
  const auto support = f::external().support();
  CHECK_FALSE(ThreadAdapter::inspect_support(part, BodyId("body.shaft"), support, cache));
  CHECK_FALSE(ThreadAdapter::inspect_support(part, BodyId("body.shaft"), support, cache,
                                             ThreadDirection::AlongSupportAxis, nullptr,
                                             prefix + 1));
  const auto inspected =
      checked(ThreadAdapter::inspect_support(part, BodyId("body.shaft"), support, cache,
                                             ThreadDirection::AlongSupportAxis, nullptr, prefix));
  CHECK(inspected.origin.x == Catch::Approx(30).margin(1e-7));
  CHECK(inspected.origin.z == Catch::Approx(0).margin(1e-7));
  CHECK(inspected.axis.z == Catch::Approx(1).margin(1e-7));
  CHECK(inspected.radius_mm == Catch::Approx(5).margin(1e-7));
  CHECK(inspected.axial_length_mm == Catch::Approx(8).margin(1e-7));
  CHECK(volume(*cache.find_body_shape(BodyId("body.shaft"))) ==
        Catch::Approx(200 * std::numbers::pi).margin(1e-6));
  const auto full = evaluate(part);
  const auto faces = checked(FinishingSelectionBuilder::build(
      part, full, BodyId("body.shaft"), PartFeatureInputRole::ShellRemovalFace));
  std::optional<FaceReference> downstream;
  for (const auto& face : faces)
    if (const auto* reference = std::get_if<FaceReference>(&face.reference))
      if (const auto* result = std::get_if<ResultFaceReference>(&reference->source());
          result && result->producer() == FeatureId("external")) {
        auto selected =
            checked(FaceReference::create_result(PartFeatureInputRole::ThreadSupportFace, *result));
        if (ThreadAdapter::inspect_support(part, BodyId("body.shaft"), selected, full)) {
          downstream = std::move(selected);
          break;
        }
      }
  REQUIRE(downstream);
  CHECK_FALSE(ThreadAdapter::inspect_support(part, BodyId("body.shaft"), *downstream, cache,
                                             ThreadDirection::AlongSupportAxis, nullptr, prefix));
  CHECK(checked(serialize_part_document_to_json(part)) == before);
  REQUIRE(part.set_parameter_value(ParameterId("depth"), checked(Quantity::length_mm(9))));
  CHECK_FALSE(ThreadAdapter::inspect_support(part, BodyId("body.shaft"), support, cache,
                                             ThreadDirection::AlongSupportAxis, nullptr, prefix));
}

TEST_CASE("Revolved source edges supply stable conical Thread roots through placement and replay",
          "[geometry][parity-b157][thread-support-query][revolve-lineage]") {
  const bool apex_first = GENERATE(false, true);
  auto part = checked(PartDocument::create(DocumentId("cone"), "Conical shaft"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  REQUIRE(part.add_datum_axis(
      checked(DatumAxis::create_explicit(DatumAxisId("cone.axis"), "Axis", {}, {0, 1, 0}))));
  auto sketch =
      checked(Sketch::create(SketchId("cone.profile"), "Profile", DatumPlaneId("datum.xy")));
  const std::vector<Point2> points = apex_first ? std::vector<Point2>{{0, 0}, {5, 2}, {0, 2}}
                                                : std::vector<Point2>{{0, 0}, {5, 0}, {0, 2}};
  std::vector<SketchEntityId> edges;
  for (std::size_t i = 0; i < points.size(); ++i) {
    edges.emplace_back("edge." + std::to_string(i));
    REQUIRE(sketch.add_entity(
        checked(LineSegment::create(edges.back(), points[i], points[(i + 1) % points.size()]))));
  }
  REQUIRE(sketch.add_profile(checked(ClosedProfile::create(ProfileId("cone.region"), edges))));
  REQUIRE(part.add_sketch(std::move(sketch)));
  const auto context = blcad::test::history_fixture::new_body(part, "cone");
  REQUIRE(part.add_revolve_feature(checked(RevolveFeature::create_revolve(
      FeatureId("cone"), "Cone",
      checked(ProfileRegionReference::create(SketchId("cone.profile"), ProfileId("cone.region"),
                                             PartFeatureInputRole::RevolveProfile)),
      checked(AxisReference::create_datum_axis(PartFeatureInputRole::RevolveAxis,
                                               DatumAxisId("cone.axis"))),
      RevolveAngleExtent::full(), context))));
  auto cache = evaluate(part);
  const auto expected_role =
      std::string("revolve/sketch/12:cone.profile/profile/11:cone.region/line/6:edge.") +
      (apex_first ? "0" : "1") + "/wall";
  const auto captured = checked(ResultFaceReference::create(FeatureId("cone"), BodyId("body.cone"),
                                                            {{FeatureId("cone"), expected_role}}));
  const auto reference =
      checked(FaceReference::create_result(PartFeatureInputRole::ThreadSupportFace, captured));
  const auto faces = checked(FinishingSelectionBuilder::build(
      part, cache, BodyId("body.cone"), PartFeatureInputRole::ShellRemovalFace));
  CHECK(std::count_if(faces.begin(), faces.end(), [&](const auto& face) {
          return face.semantic_id == "finishing/" + captured.identity();
        }) == 1);
  const auto along =
      checked(ThreadAdapter::inspect_support(part, BodyId("body.cone"), reference, cache));
  CHECK(along.origin.y == Catch::Approx(0).margin(1e-7));
  CHECK(along.axis.y == Catch::Approx(1).margin(1e-7));
  CHECK(along.radius_mm == Catch::Approx(apex_first ? 0 : 5).margin(1e-7));
  CHECK(along.axial_length_mm == Catch::Approx(2).margin(1e-7));
  CHECK(
      along.taper_angle_deg ==
      Catch::Approx((apex_first ? 1 : -1) * std::atan(2.5) * 180 / std::numbers::pi).margin(1e-7));
  for (const auto& [id, value] :
       {std::pair{"major", 10.}, {"minor", 9.5}, {"pitch", .375}, {"length", .95}})
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
  REQUIRE(part.add_parameter(checked(
      Parameter::create_angle(ParameterId("angle"), "Angle", checked(Quantity::angle_deg(60))))));
  REQUIRE(part.add_parameter(checked(Parameter::create_angle(
      ParameterId("taper"), "Taper",
      checked(Quantity::angle_deg(-std::atan(2.5) * 180 / std::numbers::pi))))));
  const ThreadParameterProfile profile{"Custom",
                                       "Cone",
                                       "Authored",
                                       ParameterId("major"),
                                       ParameterId("minor"),
                                       ParameterId("pitch"),
                                       ParameterId("angle"),
                                       ParameterId("taper")};
  const auto direction =
      apex_first ? ThreadDirection::OppositeSupportAxis : ThreadDirection::AlongSupportAxis;
  REQUIRE(part.add_thread_feature(checked(ThreadFeature::create(
      FeatureId("thread"), "Thread", BodyId("body.cone"), reference,
      checked(ThreadDefinition::create(profile, ParameterId("length"))), direction))));
  REQUIRE(part.add_body_transform(checked(BodyTransform::create_translate(
      BodyTransformId("pose"), BodyId("body.cone"), BodyTransformCoordinateSpace::World, {},
      {12, -5, 10}, false, false))));
  cache = evaluate(part);
  const auto placed = checked(
      ThreadAdapter::inspect_support(part, BodyId("body.cone"), reference, cache, direction));
  CHECK(placed.origin.x == Catch::Approx(12).margin(1e-7));
  CHECK(placed.origin.y == Catch::Approx(apex_first ? -3 : -5).margin(1e-7));
  CHECK(placed.origin.z == Catch::Approx(10).margin(1e-7));
  CHECK(placed.radius_mm == Catch::Approx(5).margin(1e-7));
  CHECK(placed.axis.y == Catch::Approx(apex_first ? -1 : 1).margin(1e-7));
  const auto expected_volume = 50 * std::numbers::pi / 3;
  CHECK(volume(*cache.find_body_shape(BodyId("body.cone"))) ==
        Catch::Approx(expected_volume).margin(1e-6));
  const auto reopened =
      checked(deserialize_part_document_from_json(checked(serialize_part_document_to_json(part))));
  const auto replayed = evaluate(reopened);
  CHECK(volume(*replayed.find_body_shape(BodyId("body.cone"))) ==
        Catch::Approx(expected_volume).margin(1e-6));
  REQUIRE(
      checked(ThreadAdapter::cosmetic_presentations(*replayed.find_body_shape(BodyId("body.cone"))))
          .size() == 1);
}
