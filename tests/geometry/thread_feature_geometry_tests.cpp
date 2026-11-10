#include "../support/thread_feature_fixture.hpp"
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
