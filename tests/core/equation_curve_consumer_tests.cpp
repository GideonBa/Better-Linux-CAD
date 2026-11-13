#include "../support/equation_curve_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace blcad;
namespace f = blcad::test::equation_fixture;
namespace {
EquationCurve circle(EquationCurveClosure closure = EquationCurveClosure::Periodic,
                     bool construction = false) {
  return f::checked(EquationCurve::create(
      SketchEntityId("circle"), EquationCurveMode::Parametric2D, {"u", CurveUnit::Scalar}, "0",
      "2*pi", {"@{radius}*cos(u)", "@{radius}*sin(u)", ""}, .001, closure, construction));
}
} // namespace
TEST_CASE("Single equation profiles require an owned closed geometric source and survive V8 replay",
          "[core][parity-b161][equation-consumers]") {
  auto part = f::part();
  auto sketch = *part.find_sketch(SketchId("planar"));
  CHECK_FALSE(ArcClosedProfile::create(ProfileId("profile"), {SketchEntityId("circle")}));
  CHECK_FALSE(ArcClosedProfile::create_for_sketch(ProfileId("profile"), {SketchEntityId("circle")},
                                                  sketch));
  REQUIRE(sketch.add_entity(circle(EquationCurveClosure::Open)));
  CHECK_FALSE(ArcClosedProfile::create_for_sketch(ProfileId("profile"), {SketchEntityId("circle")},
                                                  sketch));
  REQUIRE(sketch.replace_equation_curve(circle()));
  auto profile = f::checked(ArcClosedProfile::create_for_sketch(
      ProfileId("profile"), {SketchEntityId("circle")}, sketch));
  REQUIRE(sketch.add_profile(profile));
  REQUIRE(part.update_sketch(sketch));
  CHECK_FALSE(sketch.remove_equation_curve(SketchEntityId("circle")));
  CHECK_FALSE(sketch.replace_equation_curve(circle(EquationCurveClosure::Open)));
  CHECK_FALSE(sketch.replace_equation_curve(circle(EquationCurveClosure::Periodic, true)));
  const auto before = f::checked(serialize_part_document_to_json(part));
  auto reopened = f::checked(deserialize_part_document_from_json(before));
  CHECK(f::checked(serialize_part_document_to_json(reopened)) == before);
  CHECK(reopened.find_sketch(SketchId("planar"))
            ->find_arc_closed_profile(ProfileId("profile"))
            ->curve_segments() == std::vector{SketchEntityId("circle")});
}
TEST_CASE("Equation path sources validate closure and reject incompatible source edits atomically",
          "[core][parity-b161][equation-consumers]") {
  auto part = f::part();
  auto sketch = *part.find_sketch(SketchId("planar"));
  REQUIRE(sketch.add_entity(circle()));
  REQUIRE(part.update_sketch(sketch));
  const auto segment = f::checked(PathSegmentReference::create_planar(
      sketch.id(), SketchEntityId("circle"), PlanarPathCurveKind::EquationCurve));
  CHECK_FALSE(part.add_path_curve(
      f::checked(PathCurve::create(PathCurveId("wrong"), "Wrong", {segment}, PathClosure::Open,
                                   PathOrientationRule::MinimumTwist))));
  REQUIRE(part.add_path_curve(
      f::checked(PathCurve::create(PathCurveId("path"), "Path", {segment}, PathClosure::Closed,
                                   PathOrientationRule::MinimumTwist))));
  const auto before = f::checked(serialize_part_document_to_json(part));
  REQUIRE(sketch.replace_equation_curve(circle(EquationCurveClosure::Open)));
  CHECK_FALSE(part.update_sketch(sketch));
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
  auto reopened = f::checked(deserialize_part_document_from_json(before));
  CHECK(reopened.path_curves().size() == 1);
  CHECK(reopened.path_curves()[0].segments()[0].planar_curve_kind() ==
        PlanarPathCurveKind::EquationCurve);
  CHECK(f::checked(serialize_part_document_to_json(reopened)) == before);
}
