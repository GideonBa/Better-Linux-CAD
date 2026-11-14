#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/spatial_constraint_geometry.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::spatial_fixture;
namespace {
ShapeCache cache_for(const PartDocument& part) {
  auto cache = f::checked(ShapeCache::create(ShapeCacheId("spatial.solver")));
  (void)f::checked(GeometryRecomputeExecutor{}.execute_document(part, cache));
  return cache;
}
Sketch3D curves() {
  auto s = f::checked(Sketch3D::create(Sketch3DId("curves"), "Analytic witnesses"));
  for (const auto& [id, p] : std::vector<std::pair<std::string, Point3>>{{"a", {4, 0, 0}},
                                                                         {"b", {0, 4, 0}},
                                                                         {"c", {-4, 0, 0}},
                                                                         {"d", {4, 4, 0}},
                                                                         {"e", {-4, 4, 0}}})
    (void)f::checked(s.add_point(f::point(id, p)));
  (void)f::checked(s.add_arc(f::checked(SketchArc3D::create_three_point(
      SketchEntityId("arc"), f::local("a"), f::local("b"), f::local("c")))));
  (void)f::checked(s.add_spline(f::checked(
      SketchSpline3D::create(SketchEntityId("spline"), SketchSpline3DRepresentation::ControlPoints,
                             {f::local("a"), f::local("d"), f::local("e"), f::local("c")}, 3,
                             SketchSpline3DContinuity::Curvature))));
  auto axis = f::checked(SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.z")));
  (void)f::checked(s.add_helix(f::checked(SketchHelix3D::create(
      SketchEntityId("helix"), axis, ParameterId("radius"), ParameterId("pitch"),
      ParameterId("turns"), SketchHelix3DHandedness::RightHanded))));
  return s;
}
void coordinate_checks(Point3 actual, Point3 expected, double tolerance = 1e-7) {
  CHECK(actual.x == Catch::Approx(expected.x).margin(tolerance));
  CHECK(actual.y == Catch::Approx(expected.y).margin(tolerance));
  CHECK(actual.z == Catch::Approx(expected.z).margin(tolerance));
}
std::string snapshot(const PartDocument& part, const Sketch3D& source) {
  auto p = part;
  auto r = p.find_sketch_3d(source.id()) ? p.update_sketch_3d(source) : p.add_sketch_3d(source);
  (void)f::checked(r);
  return f::checked(serialize_part_document_to_json(p));
}
} // namespace
TEST_CASE("Spatial solver native curve queries match independent analytic arc "
          "cubic and helix jets",
          "[geometry][parity-b164][spatial-native-jets]") {
  auto part = f::part();
  auto s = curves();
  REQUIRE(part.add_sketch_3d(s));
  auto cache = cache_for(part);
  auto queries = SpatialConstraintGeometry{}.queries(part, cache);
  REQUIRE(queries.validate_sources());
  for (double u : {0., .125, .5, .875, 1.}) {
    REQUIRE(queries.prepare_candidate(s));
    const auto arc = f::checked(queries.curve(s, f::curve("arc"), u));
    const double t = std::numbers::pi * u;
    coordinate_checks(arc.point, {4 * std::cos(t), 4 * std::sin(t), 0});
    CHECK(arc.first.x == Catch::Approx(-4 * std::numbers::pi * std::sin(t)).margin(1e-7));
    CHECK(arc.second.y ==
          Catch::Approx(-4 * std::numbers::pi * std::numbers::pi * std::sin(t)).margin(1e-6));
    const auto cubic = f::checked(queries.curve(s, f::curve("spline"), u));
    coordinate_checks(cubic.point, {4 - 24 * u * u + 16 * u * u * u, 12 * u * (1 - u), 0});
    CHECK(cubic.first.x == Catch::Approx(-48 * u + 48 * u * u).margin(1e-7));
    CHECK(cubic.second.y == Catch::Approx(-24).margin(1e-7));
    const auto helix = f::checked(queries.curve(s, f::curve("helix"), u));
    // Native helix carrier chooses its established axis basis; radial distance,
    // axial rise and derivative magnitudes are independent of that basis and
    // the solver implementation.
    CHECK(std::hypot(helix.point.x, helix.point.y) == Catch::Approx(4).margin(2e-5));
    CHECK(helix.point.z == Catch::Approx(6 * u).margin(1e-6));
    CHECK(std::hypot(helix.first.x, helix.first.y) ==
          Catch::Approx(24 * std::numbers::pi).margin(5e-3));
  }
  CHECK(f::checked(queries.curve_length(s, f::curve("arc"))) ==
        Catch::Approx(4 * std::numbers::pi).margin(1e-7));
  CHECK(f::checked(queries.curve_length(s, f::curve("helix"))) ==
        Catch::Approx(3 * std::hypot(8 * std::numbers::pi, 2.)).margin(1e-4));
  CHECK(f::checked(queries.radius(s, f::curve("arc"))) == Catch::Approx(4));
  // Current candidate conversion reads the trial coordinates without Part
  // publication or cache reuse.
  auto trial = s;
  REQUIRE(trial.replace_point(f::point("b", {0, 8, 0})));
  REQUIRE(queries.prepare_candidate(trial));
  CHECK(f::checked(queries.curve(trial, f::curve("arc"), .5)).point.y ==
        Catch::Approx(8).margin(1e-7));
  CHECK(part.find_sketch_3d(s.id())
            ->find_point(SketchEntityId("b"))
            ->y()
            .explicit_coordinate()
            ->millimeters() == 4);
}
TEST_CASE("Spatial native face constraints use trimmed interiors and reject "
          "stale source results",
          "[geometry][parity-b164][spatial-face]") {
  auto part = f::part();
  auto s = f::checked(Sketch3D::create(Sketch3DId("face.point"), "Face point"));
  REQUIRE(s.add_point(f::point("p", {1, 1, 8})));
  REQUIRE(s.add_constraint(f::checked(SpatialConstraint::create(SketchConstraintId("on.face"),
                                                                SpatialConstraintKind::PointOnFace,
                                                                {f::target("p"), f::face()}))));
  REQUIRE(part.add_sketch_3d(s));
  auto cache = cache_for(part);
  auto q = SpatialConstraintGeometry{}.queries(part, cache);
  const auto solved = solve_spatial_sketch(part, s, q);
  INFO(solved.diagnostic);
  REQUIRE(solved.accepted());
  CHECK(solved.jacobian_rank == 1);
  CHECK(solved.degrees_of_freedom == 2);
  CHECK(solved.gauge_degrees_of_freedom == 2);
  CHECK(solved.sketch.find_point(SketchEntityId("p"))->z().explicit_coordinate()->millimeters() ==
        Catch::Approx(5).margin(1e-7));
  auto outside = s;
  REQUIRE(outside.replace_point(f::point("p", {20, 20, 8})));
  const auto rejected = solve_spatial_sketch(part, outside, q);
  CHECK_FALSE(rejected.accepted());
  CHECK(snapshot(part, rejected.sketch) == snapshot(part, outside));
  auto stale = part;
  REQUIRE(stale.set_parameter_value(ParameterId("depth"), f::checked(Quantity::length_mm(6))));
  const auto invalid =
      solve_spatial_sketch(stale, s, SpatialConstraintGeometry{}.queries(stale, cache));
  CHECK_FALSE(invalid.accepted());
  CHECK(snapshot(stale, invalid.sketch) == snapshot(stale, s));
}
TEST_CASE("Native spatial G2 uses oriented differential curvature and "
          "preserves impossible fixed curves",
          "[geometry][parity-b164][spatial-g2]") {
  auto part = f::part();
  auto s = f::checked(Sketch3D::create(Sketch3DId("smooth"), "Smooth"));
  const std::vector<Point3> positions{{-3, 0, 0}, {-2, 1, 0}, {-1, 1, 0}, {0, 0, 0},
                                      {0, 0, 0},  {1, -1, 0}, {2, -3, 0}, {3, -4, 0}};
  for (std::size_t i = 0; i < positions.size(); ++i)
    REQUIRE(s.add_point(f::point("p" + std::to_string(i), positions[i])));
  for (int curve : {0, 1}) {
    std::vector<SketchPointReference3D> controls;
    for (int i = 0; i < 4; ++i)
      controls.push_back(f::local(("p" + std::to_string(curve * 4 + i)).c_str()));
    REQUIRE(s.add_spline(f::checked(SketchSpline3D::create(
        SketchEntityId(curve ? "b" : "a"), SketchSpline3DRepresentation::ControlPoints, controls, 3,
        SketchSpline3DContinuity::Curvature))));
  }
  REQUIRE(s.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("smooth"), SpatialConstraintKind::Smooth,
      {f::curve("a", SpatialCurveLocation::End), f::curve("b", SpatialCurveLocation::Start)}))));
  REQUIRE(part.add_sketch_3d(s));
  auto cache = cache_for(part);
  auto queries = SpatialConstraintGeometry{}.queries(part, cache);
  const auto solved = solve_spatial_sketch(part, s, queries);
  INFO(solved.diagnostic);
  REQUIRE(solved.accepted());
  CHECK(solved.jacobian_rank == 7);
  CHECK(solved.degrees_of_freedom == 17);
  CHECK(solved.gauge_degrees_of_freedom == 6);
  auto impostor = s;
  REQUIRE(impostor.replace_point(f::point("p6", {2, -2, 0})));
  for (std::size_t i = 0; i < positions.size(); ++i) {
    const auto p = i == 6 ? Point3{2, -2, 0} : positions[i];
    REQUIRE(impostor.add_constraint(f::checked(SpatialConstraint::create(
        SketchConstraintId("fixed." + std::to_string(i)), SpatialConstraintKind::Fixed,
        {f::target(("p" + std::to_string(i)).c_str())}, {}, p))));
  }
  const auto rejected = solve_spatial_sketch(part, impostor, queries);
  CHECK_FALSE(rejected.accepted());
  CHECK(snapshot(part, rejected.sketch) == snapshot(part, impostor));
  REQUIRE(queries.prepare_candidate(impostor));
  const auto a = f::checked(queries.curve(impostor, f::curve("a"), 1)),
             b = f::checked(queries.curve(impostor, f::curve("b"), 0));
  CHECK(length(cross(a.first, b.first)) < 1e-10);
  CHECK(length(a.second) > 1);
  CHECK(length(b.second) < 1e-10);
}

TEST_CASE("Native spatial radius and length solve helix scalars and reject singular owner curves",
          "[geometry][parity-b164][spatial-native-scalars]") {
  auto part = f::part();
  auto source = curves();
  const auto old = *source.find_helix(SketchEntityId("helix"));
  const auto owned = [](double value) {
    return f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(value))));
  };
  REQUIRE(source.replace_helix(f::checked(SketchHelix3D::create_with_bindings(
      old.id(), old.axis(), owned(3), owned(1), old.turns_parameter(), old.handedness()))));
  REQUIRE(source.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("radius"), SpatialConstraintKind::Radius,
                                {f::curve("helix")}, f::length(4)))));
  REQUIRE(source.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("length"), SpatialConstraintKind::Length, {f::curve("helix")},
      f::length(3 * std::hypot(8 * std::numbers::pi, 2.))))));
  REQUIRE(part.add_sketch_3d(source));
  auto cache = cache_for(part);
  auto queries = SpatialConstraintGeometry{}.queries(part, cache);
  const auto solved = solve_spatial_sketch(part, source, queries);
  INFO(solved.diagnostic);
  REQUIRE(solved.accepted());
  CHECK(solved.jacobian_rank == 2);
  const auto* helix = solved.sketch.find_helix(old.id());
  CHECK(helix->radius_binding().explicit_length()->millimeters() == Catch::Approx(4).margin(1e-6));
  CHECK(helix->pitch_binding().explicit_length()->millimeters() == Catch::Approx(2).margin(1e-3));
  for (const auto& point : source.points()) {
    const auto* current = solved.sketch.find_point(point.id());
    REQUIRE(current);
    CHECK(current->x().explicit_coordinate()->millimeters() ==
          point.x().explicit_coordinate()->millimeters());
    CHECK(current->y().explicit_coordinate()->millimeters() ==
          point.y().explicit_coordinate()->millimeters());
    CHECK(current->z().explicit_coordinate()->millimeters() ==
          point.z().explicit_coordinate()->millimeters());
  }
  REQUIRE(queries.prepare_candidate(solved.sketch));
  const auto witness = f::checked(queries.curve(solved.sketch, f::curve("helix"), .5));
  CHECK(std::hypot(witness.point.x, witness.point.y) == Catch::Approx(4).margin(2e-5));
  CHECK(witness.point.z == Catch::Approx(3).margin(2e-3));
  auto singular = source;
  REQUIRE(singular.replace_point(f::point("b", {0, 0, 0})));
  const auto rejected = solve_spatial_sketch(part, singular, queries);
  CHECK_FALSE(rejected.accepted());
  CHECK(snapshot(part, rejected.sketch) == snapshot(part, singular));
}
