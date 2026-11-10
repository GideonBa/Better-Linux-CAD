#include "../support/hole_pattern_fixture.hpp"
#include "blcad/geometry/hole_instance_geometry.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>
#include <sstream>

using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::hole_pattern_fixture;
using f::checked;
namespace {
ShapeCache evaluate(const PartDocument& part) {
  auto cache = checked(ShapeCache::create(ShapeCacheId("hole.pattern")));
  auto result = GeometryRecomputeExecutor{}.execute_document(part, cache);
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  REQUIRE(cache.validate_complete_geometry_result(part));
  return cache;
}
double volume(const GeometryShape& shape) {
  GProp_GProps p;
  BRepGProp::VolumeProperties(*OcctShapeView::get(shape), p, 1e-9);
  return p.Mass();
}
std::string brep(const GeometryShape& shape) {
  std::ostringstream out;
  BRepTools::Write(*OcctShapeView::get(shape), out);
  return out.str();
}
HoleInstanceReference instance(PatternOccurrenceIndex index,
                               HoleReferenceRole role = HoleReferenceRole::Seat,
                               const char* placement = "first") {
  return checked(HoleInstanceReference::create(FeatureId("holes"), HolePlacementId(placement), role,
                                               HolePatternOccurrence{FeatureId("pattern"), index}));
}
void count(PartDocument& p, unsigned n) {
  REQUIRE(p.set_parameter_value(ParameterId("count"), checked(Quantity::count(n))));
}
} // namespace

TEST_CASE(
    "Rectangular Hole operation Patterns retain each placement and cut the original only once",
    "[geometry][parity-b156][hole-pattern]") {
  auto p = f::part(true);
  REQUIRE(p.add_linear_pattern_feature(f::linear()));
  auto cache = evaluate(p);
  const auto& shape = *cache.find_body_shape(BodyId("body.base"));
  REQUIRE(BRepCheck_Analyzer(*OcctShapeView::get(shape)).IsValid());
  // Eight-mm plate; twelve six-mm pilot bores with one-mm counterbores.
  CHECK(volume(shape) == Catch::Approx(38400 - 12 * 17.5 * std::numbers::pi).margin(1e-6));
  for (unsigned i = 0; i < 3; ++i)
    for (unsigned j = 0; j < 2; ++j)
      for (bool other : {false, true}) {
        const auto ref = instance({i, j}, HoleReferenceRole::Seat, other ? "other" : "first");
        const auto frame = checked(HoleInstanceGeometry::resolve(p, ref, cache));
        CHECK(frame.frame.origin.x == Catch::Approx(-20 + 12. * i).margin(1e-7));
        CHECK(frame.frame.origin.y == Catch::Approx((other ? -6. : -15.) + 15 * j).margin(1e-7));
        CHECK(frame.frame.origin.z == Catch::Approx(7).margin(1e-7));
        CHECK(frame.frame.normal.z == Catch::Approx(-1));
        CHECK(frame.radius_mm == Catch::Approx(1.5));
        const auto at = frame.frame.origin;
        CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(shape), gp_Pnt(at.x, at.y, 3), 1e-7)
                  .State() == TopAbs_OUT);
        CHECK(BRepClass3d_SolidClassifier(*OcctShapeView::get(shape), gp_Pnt(at.x, at.y, 1), 1e-7)
                  .State() == TopAbs_IN);
      }
  const auto saved = checked(serialize_part_document_to_json(p));
  auto reopened = checked(deserialize_part_document_from_json(saved));
  const auto replay = evaluate(reopened);
  CHECK(volume(*replay.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(volume(shape)).margin(1e-7));
  CHECK(checked(HoleInstanceGeometry::resolve(reopened, instance({2, 1}), replay)).frame.origin.x ==
        Catch::Approx(4));
}

TEST_CASE("Hole Pattern count one skips and restored coordinates preserve exact current references",
          "[geometry][parity-b156][hole-pattern]") {
  auto p = f::part();
  count(p, 1);
  REQUIRE(p.set_parameter_value(ParameterId("rows"), checked(Quantity::count(1))));
  REQUIRE(p.add_linear_pattern_feature(f::linear()));
  auto cache = evaluate(p);
  CHECK(OcctShapeView::get(*cache.find_feature_shape(FeatureId("holes")))
            ->IsSame(*OcctShapeView::get(*cache.find_feature_shape(FeatureId("pattern")))));
  const auto retained = instance({1, 0});
  count(p, 3);
  auto plan = checked(RecomputePlan::from_graph_and_changed_nodes(p.dependency_graph(), {"count"}));
  REQUIRE(GeometryRecomputeExecutor{}.execute_plan(p, plan, cache));
  const auto before = checked(HoleInstanceGeometry::resolve(p, retained, cache));
  CHECK(before.frame.origin.x == Catch::Approx(-8));
  auto options = f::options(false);
  options.skipped = {{2, 0}};
  REQUIRE(p.update_linear_pattern_feature(f::linear(options)));
  cache = evaluate(p);
  CHECK_FALSE(HoleInstanceGeometry::resolve(p, instance({2, 0}), cache));
  CHECK(checked(HoleInstanceGeometry::resolve(p, retained, cache)).frame.origin.x ==
        before.frame.origin.x);
  CHECK(volume(*cache.find_body_shape(BodyId("body.base"))) ==
        Catch::Approx(38400 - 2 * 17.5 * std::numbers::pi).margin(1e-6));
  count(p, 1);
  cache = evaluate(p);
  CHECK_FALSE(HoleInstanceGeometry::resolve(p, retained, cache));
  CHECK(p.find_linear_pattern_feature(FeatureId("pattern"))->placements()->skipped.size() == 1);
  count(p, 3);
  cache = evaluate(p);
  CHECK(checked(HoleInstanceGeometry::resolve(p, retained, cache)).frame.origin.x ==
        before.frame.origin.x);
  CHECK_FALSE(HoleInstanceGeometry::resolve(p, instance({2, 0}), cache));
  options.skipped.clear();
  REQUIRE(p.update_linear_pattern_feature(f::linear(options)));
  cache = evaluate(p);
  CHECK(checked(HoleInstanceGeometry::resolve(p, instance({2, 0}), cache)).frame.origin.x ==
        Catch::Approx(4));
}

TEST_CASE("Circular Hole Patterns use full-circle exclusion and partial endpoints with stable axes",
          "[geometry][parity-b156][hole-pattern]") {
  for (double angle : {120., 360.})
    for (unsigned n : {1U, 2U, 5U}) {
      CAPTURE(angle, n);
      auto p = f::part();
      count(p, n);
      REQUIRE(p.add_circular_pattern_feature(f::circular(angle)));
      const auto cache = evaluate(p);
      CHECK(volume(*cache.find_body_shape(BodyId("body.base"))) ==
            Catch::Approx(38400 - n * 17.5 * std::numbers::pi).margin(2e-6));
      for (unsigned i = 0; i < n; ++i) {
        const double theta =
            n == 1 ? 0 : std::numbers::pi / 180 * angle * i / (angle == 360 ? n : n - 1);
        const auto frame = checked(HoleInstanceGeometry::resolve(p, instance({i, 0}), cache));
        CHECK(frame.frame.origin.x ==
              Catch::Approx(-20 * std::cos(theta) + 15 * std::sin(theta)).margin(1e-6));
        CHECK(frame.frame.origin.y ==
              Catch::Approx(-20 * std::sin(theta) - 15 * std::cos(theta)).margin(1e-6));
        CHECK(frame.frame.origin.z == Catch::Approx(7).margin(1e-7));
      }
    }
}

TEST_CASE("Failed Hole Pattern copies preserve all prior native bytes and current semantic results",
          "[geometry][parity-b156][hole-pattern][hole-pattern-boundaries]") {
  auto original = f::part(true);
  REQUIRE(original.add_linear_pattern_feature(f::linear()));
  const auto initial = evaluate(original);
  const auto old = brep(*initial.find_body_shape(BodyId("body.base")));
  for (int mode = 0; mode < 5; ++mode) {
    CAPTURE(mode);
    auto candidate = original;
    auto cache = initial;
    if (mode == 0)
      REQUIRE(
          candidate.set_parameter_value(ParameterId("spacing.x"), checked(Quantity::length_mm(3))));
    if (mode == 1)
      REQUIRE(
          candidate.set_parameter_value(ParameterId("spacing.y"), checked(Quantity::length_mm(9))));
    if (mode == 2)
      count(candidate, 1024);
    if (mode == 3)
      REQUIRE(candidate.set_parameter_value(ParameterId("spacing.x"),
                                            checked(Quantity::length_mm(1e308))));
    if (mode == 4) {
      auto options = f::options();
      options.second_direction->direction = f::axis("axis.x");
      REQUIRE(candidate.update_linear_pattern_feature(f::linear(options)));
    }
    const auto intent = checked(serialize_part_document_to_json(candidate));
    const auto failed = GeometryRecomputeExecutor{}.execute_document(candidate, cache);
    REQUIRE_FALSE(failed);
    CHECK(cache.validate_complete_geometry_result(original));
    CHECK(brep(*cache.find_body_shape(BodyId("body.base"))) == old);
    CHECK(checked(serialize_part_document_to_json(candidate)) == intent);
  }
}
