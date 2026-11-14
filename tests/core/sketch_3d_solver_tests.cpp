#include "../support/spatial_constraint_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_3d_solver.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
using namespace blcad;
namespace f = blcad::test::spatial_fixture;
namespace {
Sketch3D wire(const std::vector<Point3>& points) {
  auto s = f::checked(Sketch3D::create(Sketch3DId("solve"), "Solver"));
  for (std::size_t i = 0; i < points.size(); ++i)
    (void)f::checked(s.add_point(f::point("p" + std::to_string(i), points[i])));
  return s;
}
SpatialConstraintTarget pt(int i) {
  return f::target(("p" + std::to_string(i)).c_str());
}
void distance_constraint(Sketch3D& s, const char* id, int a, int b, double d) {
  (void)f::checked(s.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId(id), SpatialConstraintKind::Distance, {pt(a), pt(b)}, f::length(d)))));
}
void fixed(Sketch3D& s, const char* id, int point, Point3 p) {
  (void)f::checked(s.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId(id), SpatialConstraintKind::Fixed, {pt(point)}, {}, p))));
}
std::string snapshot(const PartDocument& part, const Sketch3D& s) {
  auto copy = part;
  (void)f::checked(copy.add_sketch_3d(s));
  return f::checked(serialize_part_document_to_json(copy));
}
Point3 resolved(const PartDocument& p, const SketchPoint3D& v) {
  const auto c = [&](const auto& v) {
    return v.explicit_coordinate() ? v.explicit_coordinate()->millimeters()
                                   : p.find_parameter(*v.parameter())->value().millimeters();
  };
  return {c(v.x()), c(v.y()), c(v.z())};
}
void counts(const SpatialSolveResult& r, std::size_t rank, std::size_t dof, std::size_t gauge,
            std::size_t internal) {
  INFO(r.diagnostic);
  REQUIRE(r.accepted());
  CHECK(r.jacobian_rank == rank);
  CHECK(r.degrees_of_freedom == dof);
  CHECK(r.gauge_degrees_of_freedom == gauge);
  CHECK(r.internal_degrees_of_freedom == internal);
}
} // namespace
TEST_CASE("Spatial rank and local rigid gauge agree with independent "
          "elementary rigidity counts",
          "[core][parity-b164][spatial-rank]") {
  const auto part = f::part();
  counts(solve_spatial_sketch(part, wire({{2, 3, 4}})), 0, 3, 3, 0);
  auto pair = wire({{0, 0, 0}, {4, 0, 0}});
  distance_constraint(pair, "length", 0, 1, 4);
  counts(solve_spatial_sketch(part, pair), 1, 5, 5, 0);
  auto triangle = wire({{0, 0, 5}, {4, 0, 5}, {0, 3, 5}});
  counts(solve_spatial_sketch(part, triangle), 0, 9, 6, 3);
  auto rigid = triangle;
  distance_constraint(rigid, "a", 0, 1, 4);
  distance_constraint(rigid, "b", 0, 2, 3);
  distance_constraint(rigid, "c", 1, 2, 5);
  counts(solve_spatial_sketch(part, rigid), 3, 6, 6, 0);
  fixed(rigid, "anchor", 0, {0, 0, 5});
  counts(solve_spatial_sketch(part, rigid), 6, 3, 3, 0);
  for (const auto& original : triangle.points()) {
    auto p = f::checked(SketchPoint3D::create(
        original.id(), original.x(), original.y(),
        f::checked(SketchCoordinate3D::create_parameter(ParameterId("depth")))));
    REQUIRE(triangle.replace_point(p));
  }
  counts(solve_spatial_sketch(part, triangle), 0, 6, 3, 3);
  fixed(pair, "anchor", 0, {});
  counts(solve_spatial_sketch(part, pair), 4, 2, 2, 0);
}
TEST_CASE("Normalized spatial solves are deterministic across rigid transforms "
          "and six orders of scale",
          "[core][parity-b164][spatial-normalization]") {
  const auto part = f::part();
  for (double scale : {1e-6, 1., 1e6})
    for (int rotation : {0, 1, 2}) {
      const auto map = [&](Point3 p) {
        if (rotation == 0)
          return Point3{scale * p.x, scale * p.y, scale * p.z};
        if (rotation == 1)
          return Point3{scale * (20 + p.z), scale * (30 + p.x), scale * (40 + p.y)};
        const auto axis = normalized(Vector3{1, 2, 3});
        const double c = std::cos(.7), s = std::sin(.7);
        const auto crossed = cross(axis, Vector3{p.x, p.y, p.z});
        const double projected = dot(axis, Vector3{p.x, p.y, p.z}) * (1 - c);
        return Point3{scale * (20 + c * p.x + s * crossed.x + axis.x * projected),
                      scale * (30 + c * p.y + s * crossed.y + axis.y * projected),
                      scale * (40 + c * p.z + s * crossed.z + axis.z * projected)};
      };
      auto s = wire({map({0, 0, 0}), map({10, 0, 1}), map({1, 8, 2})});
      distance_constraint(s, "a", 0, 1, 4 * scale);
      distance_constraint(s, "b", 0, 2, 3 * scale);
      distance_constraint(s, "c", 1, 2, 5 * scale);
      const auto before = snapshot(part, s);
      const auto a = solve_spatial_sketch(part, s), b = solve_spatial_sketch(part, s);
      INFO(a.diagnostic);
      REQUIRE(a.accepted());
      REQUIRE(b.accepted());
      CHECK(snapshot(part, s) == before);
      CHECK(snapshot(part, a.sketch) == snapshot(part, b.sketch));
      CHECK(a.iterations == b.iterations);
      CHECK(a.evaluations == b.evaluations);
      const auto p0 = resolved(part, *a.sketch.find_point(SketchEntityId("p0"))),
                 p1 = resolved(part, *a.sketch.find_point(SketchEntityId("p1"))),
                 p2 = resolved(part, *a.sketch.find_point(SketchEntityId("p2")));
      CHECK(distance(p0, p1) / scale == Catch::Approx(4).margin(1e-6));
      CHECK(distance(p0, p2) / scale == Catch::Approx(3).margin(1e-6));
      CHECK(distance(p1, p2) / scale == Catch::Approx(5).margin(1e-6));
      CHECK(a.length_scale_mm / scale == Catch::Approx(2 * std::sqrt(425.) / 3).margin(1e-10));
      counts(a, 3, 6, 6, 0);
    }
}
TEST_CASE("Spatial conflict nonconvergence and invalid capabilities return the "
          "exact authored source",
          "[core][parity-b164][spatial-rollback]") {
  const auto part = f::part();
  auto s = wire({{2, 3, 4}});
  fixed(s, "a", 0, {});
  fixed(s, "b", 0, {1, 0, 0});
  const auto before = snapshot(part, s);
  const auto result = solve_spatial_sketch(part, s);
  CHECK_FALSE(result.accepted());
  CHECK(result.status == SketchSolveStatus::Conflicting);
  CHECK(snapshot(part, result.sketch) == before);
  CHECK(snapshot(part, s) == before);
  CHECK(result.constraints.size() == 2);
  auto distance = wire({{0, 0, 0}, {20, 1, 0}});
  distance_constraint(distance, "distance", 0, 1, 4);
  for (int failure : {0, 1, 2, 3, 4}) {
    SpatialSolverOptions o;
    if (failure == 0)
      o.numeric.maximum_iterations = 1;
    if (failure == 1)
      o.maximum_evaluations = 1;
    if (failure == 2)
      o.numeric.finite_difference_relative_step = 0;
    if (failure == 3)
      o.maximum_variables = 1;
    if (failure == 4)
      o.maximum_entities = 1;
    const auto r = solve_spatial_sketch(part, distance, {}, o);
    CHECK_FALSE(r.accepted());
    CHECK(snapshot(part, r.sketch) == snapshot(part, distance));
  }
  SpatialSolverGeometry lost;
  lost.validate_sources = []() {
    return Result<std::size_t>::failure(Error::validation("supplier", "Supplier bytes changed"));
  };
  const auto stale = solve_spatial_sketch(part, distance, lost);
  CHECK_FALSE(stale.accepted());
  CHECK(snapshot(part, stale.sketch) == snapshot(part, distance));
}
TEST_CASE("Spatial duplicate attribution uses deterministic IDs and physical rank",
          "[core][parity-b164][spatial-redundancy]") {
  const auto part = f::part();
  auto s = wire({{0, 0, 0}, {4, 0, 0}});
  distance_constraint(s, "z", 0, 1, 4);
  distance_constraint(s, "a", 0, 1, 4);
  const auto r = solve_spatial_sketch(part, s);
  counts(r, 1, 5, 5, 0);
  REQUIRE(r.constraints.size() == 2);
  CHECK(r.constraints[0].id == SketchConstraintId("a"));
  CHECK_FALSE(r.constraints[0].redundant);
  CHECK(r.constraints[1].redundant);
  CHECK(r.status == SketchSolveStatus::Redundant);
}
TEST_CASE("Auxiliary curve coordinates preserve branch intent and expose "
          "physical rather than auxiliary redundancy",
          "[core][parity-b164][spatial-auxiliary]") {
  const auto part = f::part();
  auto s = wire({{.2, 2, 3}});
  const auto external = f::checked(SpatialConstraintTarget::external_curve(f::checked(
      PathSegmentReference::create_construction_line(ConstructionLineId("external.line")))));
  // The external source belongs to a separate Part authority, established
  // before solving.
  auto doc = part;
  auto a = f::checked(ConstructionPoint::create_explicit(ConstructionPointId("line.a"), "A", {}));
  auto b =
      f::checked(ConstructionPoint::create_explicit(ConstructionPointId("line.b"), "B", {1, 0, 0}));
  REQUIRE(doc.add_construction_point(a));
  REQUIRE(doc.add_construction_point(b));
  REQUIRE(doc.add_construction_line(f::checked(ConstructionLine::create_explicit(
      ConstructionLineId("external.line"), "Line", {}, {1, 0, 0}))));
  REQUIRE(s.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("on"), SpatialConstraintKind::PointOnCurve, {pt(0), external}, {}, {},
      SpatialAngleConvention::Unsigned, .25))));
  SpatialSolverGeometry geometry;
  geometry.curve = [](const Sketch3D&, const SpatialConstraintTarget&, double u) {
    return Result<SpatialCurveJet>::success({{u, 0, 0}, {1, 0, 0}, {}});
  };
  const auto r = solve_spatial_sketch(doc, s, geometry);
  counts(r, 3, 1, 1, 0);
  CHECK(r.sketch.constraints()[0].curve_parameter_seed() == .25);
  CHECK(r.sketch.find_point(SketchEntityId("p0"))->x().explicit_coordinate()->millimeters() ==
        Catch::Approx(.2).margin(1e-12));
  const auto repeated = solve_spatial_sketch(doc, r.sketch, geometry);
  REQUIRE(repeated.accepted());
  CHECK(snapshot(doc, repeated.sketch) == snapshot(doc, r.sketch));
  CHECK(repeated.iterations == 0);
  auto self = wire({{0, 0, 0}, {4, 0, 0}});
  REQUIRE(self.add_line(f::checked(
      SketchLine3D::create(SketchEntityId("line"), SketchEntityId("p0"), SketchEntityId("p1")))));
  REQUIRE(self.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("self"), SpatialConstraintKind::PointOnCurve, {pt(0), f::curve("line")},
      {}, {}, SpatialAngleConvention::Unsigned, 0))));
  const auto redundant = solve_spatial_sketch(part, self);
  counts(redundant, 1, 6, 5, 1);
  REQUIRE(redundant.constraints.size() == 1);
  CHECK(redundant.constraints[0].redundant);
}
TEST_CASE("Spatial directional relations retain unsigned endpoints and "
          "oriented projected angle conventions",
          "[core][parity-b164][spatial-angle]") {
  const auto part = f::part();
  SpatialSolverGeometry q;
  q.axis = [](const AxisReference& r) {
    return Result<SpatialAxisFrame>::success(
        {{}, r.source_node_id() == "axis.z" ? Vector3{0, 0, 1} : Vector3{1, 0, 0}});
  };
  q.plane = [](const PlaneReference&) {
    return Result<SpatialPlaneFrame>::success({{}, {0, 0, 1}});
  };
  const auto line = [](Point3 direction) {
    auto s = wire({{}, direction});
    (void)f::checked(s.add_line(f::checked(
        SketchLine3D::create(SketchEntityId("line"), SketchEntityId("p0"), SketchEntityId("p1")))));
    return s;
  };
  for (double angle : {0., 90., 180.}) {
    auto s = line(angle == 0 ? Point3{0, 0, 1} : angle == 180 ? Point3{0, 0, -1} : Point3{1, 0, 0});
    REQUIRE(s.add_constraint(f::checked(
        SpatialConstraint::create(SketchConstraintId("angle"), SpatialConstraintKind::Angle,
                                  {f::curve("line"), f::axis()}, f::angle(angle)))));
    const auto r = solve_spatial_sketch(part, s, q);
    INFO(r.diagnostic);
    REQUIRE(r.accepted());
    CHECK(r.jacobian_rank == (angle == 90 ? 1 : 2));
  }
  auto directional = part;
  REQUIRE(directional.add_datum_axis(
      f::checked(DatumAxis::create_explicit(DatumAxisId("axis.x"), "X", {}, {1, 0, 0}))));
  auto x = f::checked(SpatialConstraintTarget::axis(f::checked(AxisReference::create_datum_axis(
      PartFeatureInputRole::SpatialConstraint, DatumAxisId("axis.x")))));
  for (double target : {-90., 180.}) {
    auto signed_line = line(target == -90 ? Point3{0, 1, 2} : Point3{-1, 0, 2});
    REQUIRE(signed_line.add_constraint(f::checked(
        SpatialConstraint::create(SketchConstraintId("signed"), SpatialConstraintKind::Angle,
                                  {f::curve("line"), x, f::axis()}, f::angle(target), {},
                                  SpatialAngleConvention::OrientedAboutAxis))));
    const auto signed_result = solve_spatial_sketch(directional, signed_line, q);
    INFO(signed_result.diagnostic);
    REQUIRE(signed_result.accepted());
    CHECK(signed_result.jacobian_rank == 1);
  }
  auto singular = line({0, 0, 1});
  REQUIRE(singular.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("signed"), SpatialConstraintKind::Angle, {f::curve("line"), x, f::axis()},
      f::angle(-90), {}, SpatialAngleConvention::OrientedAboutAxis))));
  const auto invalid = solve_spatial_sketch(directional, singular, q);
  CHECK_FALSE(invalid.accepted());
  CHECK(snapshot(directional, invalid.sketch) == snapshot(directional, singular));
  auto s = line({1, 2, 3});
  REQUIRE(s.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("parallel.plane"), SpatialConstraintKind::PlaneParallel,
      {f::curve("line"), f::plane()}))));
  const auto parallel = solve_spatial_sketch(part, s, q);
  REQUIRE(parallel.accepted());
  CHECK(parallel.jacobian_rank == 1);
  const auto a = resolved(part, *parallel.sketch.find_point(SketchEntityId("p0"))),
             b = resolved(part, *parallel.sketch.find_point(SketchEntityId("p1")));
  CHECK(std::abs(a.z - b.z) < 1e-7);
}
TEST_CASE("Spatial solving preserves immutable coordinate formulas and integer turns",
          "[core][parity-b164][spatial-bindings]") {
  auto part = f::part();
  REQUIRE(part.add_expression_parameter(ParameterId("coordinate.bound"), "Coordinate bound",
                                        ParameterType::Length, "width / 2"));
  auto s = wire({{0, 0, 0}, {10, 0, 0}});
  auto p = *s.find_point(SketchEntityId("p0"));
  REQUIRE(s.replace_point(f::checked(SketchPoint3D::create(
      p.id(), f::checked(SketchCoordinate3D::create_parameter(ParameterId("coordinate.bound"))),
      p.y(), p.z()))));
  fixed(s, "anchor", 0, {4, 0, 0});
  distance_constraint(s, "length", 0, 1, 3);
  const auto before = f::checked(serialize_part_document_to_json(part));
  const auto r = solve_spatial_sketch(part, s);
  INFO(r.diagnostic);
  REQUIRE(r.accepted());
  CHECK(r.sketch.find_point(SketchEntityId("p0"))->x().parameter() ==
        ParameterId("coordinate.bound"));
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
  CHECK(r.variable_order.size() == 5);
  CHECK(r.degrees_of_freedom == 2);
}

TEST_CASE("Spatial helix scalars solve independently of immutable integer turns",
          "[core][parity-b164][spatial-scalars]") {
  const auto part = f::part();
  const auto before = f::checked(serialize_part_document_to_json(part));
  auto s = f::checked(Sketch3D::create(Sketch3DId("owned"), "Owned helix"));
  const auto owned = [](double value) {
    return f::checked(SketchLength3D::create_explicit(f::checked(Quantity::length_mm(value))));
  };
  REQUIRE(s.add_helix(f::checked(SketchHelix3D::create_with_bindings(
      SketchEntityId("helix"),
      f::checked(SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.z"))), owned(4), owned(2),
      ParameterId("turns"), SketchHelix3DHandedness::RightHanded))));
  REQUIRE(s.add_constraint(f::checked(
      SpatialConstraint::create(SketchConstraintId("radius"), SpatialConstraintKind::Radius,
                                {f::curve("helix")}, f::length(6)))));
  REQUIRE(s.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("length"), SpatialConstraintKind::Length, {f::curve("helix")},
      f::length(3 * std::hypot(12 * std::numbers::pi, 4.))))));
  SpatialSolverGeometry query;
  query.radius = [&](const Sketch3D& candidate, const SpatialConstraintTarget&) {
    const auto* h = candidate.find_helix(SketchEntityId("helix"));
    return Result<double>::success(f::checked(h->radius_binding().resolve(part)).millimeters());
  };
  query.curve_length = [&](const Sketch3D& candidate, const SpatialConstraintTarget&) {
    const auto* h = candidate.find_helix(SketchEntityId("helix"));
    return Result<double>::success(
        3 * std::hypot(2 * std::numbers::pi *
                           f::checked(h->radius_binding().resolve(part)).millimeters(),
                       f::checked(h->pitch_binding().resolve(part)).millimeters()));
  };
  const auto result = solve_spatial_sketch(part, s, query);
  counts(result, 2, 0, 0, 0);
  REQUIRE(result.variable_order.size() == 2);
  const auto* helix = result.sketch.find_helix(SketchEntityId("helix"));
  CHECK(helix->radius_binding().explicit_length()->millimeters() == Catch::Approx(6).margin(1e-6));
  CHECK(helix->pitch_binding().explicit_length()->millimeters() == Catch::Approx(4).margin(1e-6));
  CHECK(helix->turns_parameter() == ParameterId("turns"));
  CHECK(f::checked(serialize_part_document_to_json(part)) == before);
}

TEST_CASE("Spatial positional and directional relations have independently known residual ranks",
          "[core][parity-b164][spatial-relations]") {
  const auto part = f::part();
  SpatialSolverGeometry query;
  query.axis = [](const AxisReference&) {
    return Result<SpatialAxisFrame>::success({{}, {0, 0, 1}});
  };
  query.plane = [](const PlaneReference&) {
    return Result<SpatialPlaneFrame>::success({{}, {0, 0, 1}});
  };
  for (const auto kind :
       {SpatialConstraintKind::Coincident, SpatialConstraintKind::AxisParallel,
        SpatialConstraintKind::Parallel, SpatialConstraintKind::Perpendicular,
        SpatialConstraintKind::PointOnLine, SpatialConstraintKind::PointOnPlane}) {
    INFO(to_string(kind));
    const bool point_relation =
        kind == SpatialConstraintKind::PointOnLine || kind == SpatialConstraintKind::PointOnPlane;
    auto s = point_relation ? wire({{3, 2, 4}}) : wire({{1, 2, 3}, {4, 5, 9}});
    if (!point_relation && kind != SpatialConstraintKind::Coincident)
      REQUIRE(s.add_line(f::checked(SketchLine3D::create(
          SketchEntityId("line"), SketchEntityId("p0"), SketchEntityId("p1")))));
    std::vector<SpatialConstraintTarget> targets;
    std::size_t expected_rank = 0, dof = 0, gauge = 0, internal = 0;
    if (kind == SpatialConstraintKind::Coincident) {
      targets = {pt(0), pt(1)};
      expected_rank = 3;
      dof = 3;
      gauge = 3;
    } else if (kind == SpatialConstraintKind::PointOnLine) {
      targets = {pt(0), f::axis()};
      expected_rank = 2;
      dof = 1;
      gauge = 1;
    } else if (kind == SpatialConstraintKind::PointOnPlane) {
      targets = {pt(0), f::plane()};
      expected_rank = 1;
      dof = 2;
      gauge = 2;
    } else {
      targets = {f::curve("line"), f::axis()};
      const bool perpendicular = kind == SpatialConstraintKind::Perpendicular;
      expected_rank = perpendicular ? 1 : 2;
      dof = 6 - expected_rank;
      gauge = perpendicular ? 4 : 3;
      internal = 1;
    }
    REQUIRE(s.add_constraint(
        f::checked(SpatialConstraint::create(SketchConstraintId("relation"), kind, targets))));
    const auto result = solve_spatial_sketch(part, s, query);
    counts(result, expected_rank, dof, gauge, internal);
    const auto a = resolved(part, *result.sketch.find_point(SketchEntityId("p0")));
    if (kind == SpatialConstraintKind::Coincident)
      CHECK(distance(a, resolved(part, *result.sketch.find_point(SketchEntityId("p1")))) < 1e-7);
    if (kind == SpatialConstraintKind::PointOnLine)
      CHECK(std::hypot(a.x, a.y) < 1e-7);
    if (kind == SpatialConstraintKind::PointOnPlane)
      CHECK(std::abs(a.z) < 1e-7);
  }
  auto tangent = wire({{0, 0, 0}, {1, 0, 0}, {1, 0, 0}, {2, 1, 1}});
  REQUIRE(tangent.add_line(f::checked(
      SketchLine3D::create(SketchEntityId("a"), SketchEntityId("p0"), SketchEntityId("p1")))));
  REQUIRE(tangent.add_line(f::checked(
      SketchLine3D::create(SketchEntityId("b"), SketchEntityId("p2"), SketchEntityId("p3")))));
  REQUIRE(tangent.add_constraint(f::checked(SpatialConstraint::create(
      SketchConstraintId("tangent"), SpatialConstraintKind::Tangent,
      {f::curve("a", SpatialCurveLocation::End), f::curve("b", SpatialCurveLocation::Start)}))));
  counts(solve_spatial_sketch(part, tangent), 5, 7, 5, 2);
}
