#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_constraint_solver.hpp"
#include "blcad/core/sketch_modify.hpp"
#include "blcad/core/sketch_spline_edit.hpp"
#include "blcad/core/sketch_topology_json.hpp"
#include "blcad/core/sketch_topology_part_document.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <numbers>

using namespace blcad;

namespace {
SplineSegment quarter() {
  auto curve = SplineSegment::create_elliptical_arc(SketchEntityId("ellipse"), {3, -4}, {12, 5}, 7,
                                                    0, std::numbers::pi / 2);
  REQUIRE(curve);
  return curve.value();
}
void on_ellipse(Point2 point) {
  const Point2 d{point.x - 3, point.y + 4};
  const double x = (12 * d.x + 5 * d.y) / 13;
  const double y = (-5 * d.x + 12 * d.y) / 13;
  CHECK(x * x / (13 * 13) + y * y / 49 == Catch::Approx(1).margin(2e-12));
}
} // namespace

TEST_CASE("Rational ellipse spans satisfy the analytic conic and exact interval algebra",
          "[core][sketch-conics]") {
  const auto segment = quarter();
  REQUIRE(segment.rational());
  const auto curve = segment.curve();
  const auto cut = curve.interval(.23, .87);
  const auto reversed = curve.interval(1, 0);
  for (int i = 0; i <= 100; ++i) {
    const double t = i / 100.0;
    on_ellipse(curve.point(t));
    on_ellipse(cut.point(t));
    const auto expected = curve.point(.23 + .64 * t);
    CHECK(cut.point(t).x == Catch::Approx(expected.x).margin(1e-11));
    CHECK(cut.point(t).y == Catch::Approx(expected.y).margin(1e-11));
    CHECK(reversed.point(t).x == Catch::Approx(curve.point(1 - t).x).margin(1e-11));
    CHECK(reversed.point(t).y == Catch::Approx(curve.point(1 - t).y).margin(1e-11));
  }
  const double t = .37, h = 1e-5;
  const auto a = curve.point(t - h), b = curve.point(t + h);
  CHECK(curve.derivative(t).x == Catch::Approx((b.x - a.x) / (2 * h)).margin(1e-7));
  CHECK(curve.derivative(t).y == Catch::Approx((b.y - a.y) / (2 * h)).margin(1e-7));
  const auto da = curve.derivative(t - h), db = curve.derivative(t + h);
  CHECK(curve.second_derivative(t).x == Catch::Approx((db.x - da.x) / (2 * h)).margin(1e-7));
  CHECK(curve.second_derivative(t).y == Catch::Approx((db.y - da.y) / (2 * h)).margin(1e-7));
}

TEST_CASE("Rational conics survive document and topology roundtrips, materialization and splitting",
          "[core][sketch-conics]") {
  auto part = PartDocument::create(DocumentId("conics"), "Conics").value();
  REQUIRE(part.add_datum_plane(DatumPlane::xy().value()));
  auto sketch = Sketch::create(SketchId("sketch"), "Sketch", DatumPlaneId("datum.xy")).value();
  const auto segment = quarter();
  REQUIRE(sketch.add_entity(segment));
  REQUIRE(part.add_sketch(sketch));
  auto json = serialize_part_document_to_json(part);
  REQUIRE(json);
  CHECK(json.value().find("rational_cubic_bezier") != std::string::npos);
  auto restored = deserialize_part_document_from_json(json.value());
  REQUIRE(restored);
  const auto* restored_curve =
      restored.value().find_sketch(sketch.id())->find_spline_segment(segment.id());
  REQUIRE(restored_curve);
  CHECK(restored_curve->weights() == segment.weights());
  for (const auto& weights : {nlohmann::json::array({1, 1, 1}),
                              nlohmann::json::array({1, 1, 1, 1, 1}),
                              nlohmann::json::array({1, 0, 1, 1}), nlohmann::json("invalid")}) {
    auto malformed = nlohmann::json::parse(json.value());
    malformed["sketches"][0]["spline_segments"][0]["weights"] = weights;
    CHECK_FALSE(deserialize_part_document_from_json(malformed.dump()));
  }

  auto topology = SketchTopology::migrate_legacy(sketch);
  REQUIRE(topology);
  auto topology_json = serialize_sketch_topology_to_json(topology.value());
  REQUIRE(topology_json);
  CHECK(topology_json.value().find("rational_spline") != std::string::npos);
  auto restored_topology = deserialize_sketch_topology_from_json(topology_json.value());
  REQUIRE(restored_topology);
  auto materialized =
      SketchTopologyLegacyMaterializer{}.materialize(sketch, restored_topology.value());
  REQUIRE(materialized);
  CHECK(materialized.value().spline_segments().front().weights() == segment.weights());
  auto split = SketchModifyService::split(sketch, segment.id(), segment.curve().point(.42));
  REQUIRE(split);
  REQUIRE(split.value().sketch.spline_segments().size() == 2U);
  CHECK(split.value().sketch.spline_segments().front().end().x ==
        Catch::Approx(segment.curve().point(.42).x).margin(1e-10));
  for (const auto& span : split.value().sketch.spline_segments())
    for (double t : {0.0, .13, .37, .68, 1.0})
      on_ellipse(span.curve().point(t));
}

TEST_CASE("Conic validation rejects singular representations and keeps polynomial JSON compatible",
          "[core][sketch-conics]") {
  for (double weight :
       {0., -1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    CHECK_FALSE(SplineSegment::create_cubic_bezier(SketchEntityId("bad"), {0, 0}, {1, 0}, {1, 1},
                                                   {0, 1}, {1, weight, 1, 1}));
  CHECK_FALSE(SplineSegment::create_elliptical_arc(SketchEntityId("bad"), {}, {}, 1, 0, 1));
  CHECK_FALSE(SplineSegment::create_elliptical_arc(SketchEntityId("bad"), {}, {1, 0}, 0, 0, 1));
  CHECK_FALSE(SplineSegment::create_elliptical_arc(SketchEntityId("bad"), {}, {1, 0}, 1, 0, 0));
  CHECK_FALSE(SplineSegment::create_elliptical_arc(SketchEntityId("bad"), {}, {1, 0}, 1, 0, 3));
  auto ordinary = SplineSegment::create_cubic_bezier(SketchEntityId("ordinary"), {0, 0}, {1, 0},
                                                     {1, 1}, {0, 1});
  REQUIRE(ordinary);
  CHECK_FALSE(ordinary.value().rational());
}

TEST_CASE("Point-on-curve solve respects rational weights without distorting a valid ellipse",
          "[core][sketch-conics]") {
  auto sketch =
      Sketch::create(SketchId("rational.solve"), "Solve", DatumPlaneId("datum.xy")).value();
  const auto span = quarter();
  REQUIRE(sketch.add_entity(span));
  REQUIRE(sketch.add_entity(
      SketchPoint::create(SketchEntityId("contact"), span.curve().point(.37)).value()));
  auto topology = SketchTopology::migrate_legacy(sketch).value();
  const auto* point = topology.find_entity("entity/contact");
  REQUIRE(point);
  auto constraint =
      SketchSolverConstraint::create("on.ellipse", SketchSolverConstraintKind::PointOnObject,
                                     {SketchSolverTarget::point(point->points().front()).value(),
                                      SketchSolverTarget::entity("entity/ellipse").value()});
  REQUIRE(constraint);
  auto system = SketchConstraintSystem::create(sketch.id(), {constraint.value()});
  REQUIRE(system);
  auto solved = SketchConstraintSolver{}.solve(topology, system.value());
  REQUIRE(solved);
  CHECK(solved.value().status == SketchSolveStatus::UnderConstrained);
  CHECK(solved.value().iterations == 0U);
  CHECK(solved.value().residuals.initial_rms < 1e-10);
  auto materialized =
      SketchTopologyLegacyMaterializer{}.materialize(sketch, solved.value().topology);
  REQUIRE(materialized);
  CHECK(materialized.value().spline_segments().front().weights() == span.weights());
  on_ellipse(materialized.value().spline_segments().front().curve().point(.37));
}
