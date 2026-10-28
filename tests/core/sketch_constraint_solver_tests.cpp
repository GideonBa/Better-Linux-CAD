#include "blcad/core/part_document.hpp"
#include "blcad/core/sketch_constraint_solver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;

namespace {

SketchTopologyPoint make_point(std::string id, Point2 position) {
  return SketchTopologyPoint::create(SketchPointId(std::move(id)), position).value();
}

SketchTopologyEntity make_entity(std::string id, SketchTopologyEntityKind kind,
                                 std::vector<SketchPointId> points) {
  return SketchTopologyEntity::create(std::move(id), kind, std::move(points)).value();
}

SketchSolverTarget point_target(std::string id) {
  return SketchSolverTarget::point(SketchPointId(std::move(id))).value();
}

SketchSolverTarget entity_target(std::string id) {
  return SketchSolverTarget::entity(std::move(id)).value();
}

SketchSolverConstraint make_constraint(std::string id, SketchSolverConstraintKind kind,
                                       std::vector<SketchSolverTarget> targets,
                                       std::optional<double> value = std::nullopt) {
  return SketchSolverConstraint::create(std::move(id), kind, std::move(targets), value).value();
}

SketchTopology make_line_topology(Point2 start = {0.0, 0.0}, Point2 end = {8.0, 2.0}) {
  std::vector<SketchTopologyPoint> points;
  points.push_back(make_point("point.b", end));
  points.push_back(make_point("point.a", start));
  std::vector<SketchTopologyEntity> entities;
  entities.push_back(make_entity("entity/line.a", SketchTopologyEntityKind::Line,
                                 {SketchPointId("point.a"), SketchPointId("point.b")}));
  return SketchTopology::create(SketchId("sketch.solver"), std::move(points),
                                std::move(entities))
      .value();
}

} // namespace

TEST_CASE("Sketch solver freezes canonical variable and constraint order",
          "[core][sketch-solver][core][sketch-dof]") {
  const SketchTopology topology = make_line_topology();
  auto system = SketchConstraintSystem::create(
      topology.sketch(),
      {make_constraint("z.horizontal", SketchSolverConstraintKind::Horizontal,
                       {entity_target("entity/line.a")}),
       make_constraint("a.fixed", SketchSolverConstraintKind::Fixed,
                       {point_target("point.a")})});
  REQUIRE(system);
  REQUIRE(system.value().constraints().size() == 2U);
  CHECK(system.value().constraints()[0].id() == "a.fixed");
  CHECK(system.value().constraints()[1].id() == "z.horizontal");

  auto solved = SketchConstraintSolver{}.solve(topology, system.value());
  REQUIRE(solved);
  CHECK(solved.value().status == SketchSolveStatus::UnderConstrained);
  REQUIRE(solved.value().variable_order.size() == 4U);
  CHECK(solved.value().variable_order[0] ==
        SketchSolverVariable{SketchPointId("point.a"), SketchSolverVariableAxis::X});
  CHECK(solved.value().variable_order[1] ==
        SketchSolverVariable{SketchPointId("point.a"), SketchSolverVariableAxis::Y});
  CHECK(solved.value().variable_order[2] ==
        SketchSolverVariable{SketchPointId("point.b"), SketchSolverVariableAxis::X});
  CHECK(solved.value().variable_order[3] ==
        SketchSolverVariable{SketchPointId("point.b"), SketchSolverVariableAxis::Y});
  CHECK(solved.value().jacobian_rank == 3U);
  CHECK(solved.value().remaining_dof == 1U);
  CHECK(solved.value().residuals.final_rms <= 1.0e-9);
}

TEST_CASE("Sketch solver converges a fully constrained line",
          "[core][sketch-solver][core][sketch-dof]") {
  const SketchTopology topology = make_line_topology();
  auto system = SketchConstraintSystem::create(
      topology.sketch(),
      {make_constraint("constraint.fixed", SketchSolverConstraintKind::Fixed,
                       {point_target("point.a")}),
       make_constraint("constraint.horizontal", SketchSolverConstraintKind::Horizontal,
                       {entity_target("entity/line.a")}),
       make_constraint("dimension.width", SketchSolverConstraintKind::HorizontalDistance,
                       {point_target("point.a"), point_target("point.b")}, 10.0)});
  REQUIRE(system);

  auto solved = SketchConstraintSolver{}.solve(topology, system.value());
  REQUIRE(solved);
  CHECK(solved.value().status == SketchSolveStatus::FullyConstrained);
  CHECK(solved.value().jacobian_rank == 4U);
  CHECK(solved.value().remaining_dof == 0U);
  const auto* end = solved.value().topology.find_point(SketchPointId("point.b"));
  REQUIRE(end != nullptr);
  CHECK(end->position().x == Catch::Approx(10.0).margin(1.0e-7));
  CHECK(end->position().y == Catch::Approx(0.0).margin(1.0e-7));
  CHECK(solved.value().residuals.final_rms <= 1.0e-9);
}

TEST_CASE("Explicit coincidence groups have exact coordinates at every model scale",
          "[core][sketch-solver][coincident-closure]") {
  for (double scale : {0.001, 1.0, 1000.0, 1000000.0}) {
    for (double gap : {1.0, 0.001, 0.000001, 0.00000005, 0.0}) {
      CAPTURE(scale, gap);
      const auto topology =
          SketchTopology::create(SketchId("exact"),
                                 {make_point("a", {0.0, 0.0}), make_point("b", {0.0, gap * scale}),
                                  make_point("c", {gap * scale, gap * scale}),
                                  make_point("unrelated", {100.0 * scale, 50.0 * scale})},
                                 {})
              .value();
      const auto system =
          SketchConstraintSystem::create(
              topology.sketch(), {make_constraint("a-b", SketchSolverConstraintKind::Coincident,
                                                  {point_target("a"), point_target("b")}),
                                  make_constraint("b-c", SketchSolverConstraintKind::Coincident,
                                                  {point_target("b"), point_target("c")})})
              .value();
      const auto solved = SketchConstraintSolver{}.solve(topology, system);
      REQUIRE(solved);
      CHECK(solved.value().status == SketchSolveStatus::UnderConstrained);
      const auto& result = solved.value().topology;
      CHECK(result.find_point(SketchPointId("a"))->position() ==
            result.find_point(SketchPointId("b"))->position());
      CHECK(result.find_point(SketchPointId("b"))->position() ==
            result.find_point(SketchPointId("c"))->position());
      CHECK(*result.find_point(SketchPointId("unrelated")) ==
            *topology.find_point(SketchPointId("unrelated")));
      CHECK(solved.value().remaining_dof == 4U);
      CHECK(solved.value().residuals.final_max_abs == 0.0);
    }
  }
}

TEST_CASE("Exact coincidence respects anchored points and simultaneous dimensions",
          "[core][sketch-solver][coincident-closure]") {
  for (bool reference : {false, true}) {
    CAPTURE(reference);
    const Point2 anchor{4.0, -2.0};
    const auto topology =
        SketchTopology::create(
            SketchId("anchored"),
            {SketchTopologyPoint::create(SketchPointId("anchor"), anchor, {.reference = reference})
                 .value(),
             make_point("linked", {4.0, -2.00000005}), make_point("end", {8.0, 3.0})},
            {make_entity("line", SketchTopologyEntityKind::Line,
                         {SketchPointId("linked"), SketchPointId("end")})})
            .value();
    std::vector<SketchSolverConstraint> constraints{
        make_constraint("join", SketchSolverConstraintKind::Coincident,
                        {point_target("anchor"), point_target("linked")}),
        make_constraint("horizontal", SketchSolverConstraintKind::Horizontal,
                        {entity_target("line")}),
        make_constraint("length", SketchSolverConstraintKind::HorizontalDistance,
                        {point_target("linked"), point_target("end")}, 12.0)};
    if (!reference)
      constraints.push_back(
          make_constraint("fixed", SketchSolverConstraintKind::Fixed, {point_target("anchor")}));
    const auto system = SketchConstraintSystem::create(topology.sketch(), constraints).value();
    const auto solved = SketchConstraintSolver{}.solve(topology, system);
    REQUIRE(solved);
    CHECK(solved.value().status == SketchSolveStatus::FullyConstrained);
    CHECK(solved.value().topology.find_point(SketchPointId("anchor"))->position() == anchor);
    CHECK(solved.value().topology.find_point(SketchPointId("linked"))->position() == anchor);
    const auto end = solved.value().topology.find_point(SketchPointId("end"))->position();
    CHECK(end.x == Catch::Approx(16.0).margin(1.0e-7));
    CHECK(end.y == Catch::Approx(-2.0).margin(1.0e-7));
  }
}

TEST_CASE("Coincidence rejects distinct fixed anchors even below normalized convergence",
          "[core][sketch-solver][coincident-closure][sketch-conflict-diagnostics]") {
  const auto topology = make_line_topology({0.0, 0.0}, {0.0, 0.00000005});
  const auto system =
      SketchConstraintSystem::create(
          topology.sketch(),
          {make_constraint("a.fixed", SketchSolverConstraintKind::Fixed, {point_target("point.a")}),
           make_constraint("b.fixed", SketchSolverConstraintKind::Fixed, {point_target("point.b")}),
           make_constraint("join", SketchSolverConstraintKind::Coincident,
                           {point_target("point.a"), point_target("point.b")})})
          .value();
  const auto solved = SketchConstraintSolver{}.solve(topology, system);
  REQUIRE(solved);
  CHECK(solved.value().status == SketchSolveStatus::Conflicting);
  CHECK(solved.value().topology == topology);
  CHECK(solved.value().conflicting_constraint_ids ==
        std::vector<std::string>{"a.fixed", "b.fixed", "join"});
}

TEST_CASE("Sketch solver evaluates every Block 109 initial residual family",
          "[core][sketch-solver]") {
  std::vector<SketchTopologyPoint> points{
      make_point("a0", {0.0, 0.0}),       make_point("a1", {10.0, 0.0}),
      make_point("b0", {0.0, 5.0}),       make_point("b1", {10.0, 5.0}),
      make_point("mid", {5.0, 0.0}),      make_point("sym.a", {2.0, 2.0}),
      make_point("sym.b", {2.0, -2.0}),   make_point("dup", {0.0, 0.0}),
      make_point("col.0", {2.0, 0.0}),    make_point("col.1", {7.0, 0.0}),
      make_point("on", {3.0, 0.0}),       make_point("tan.end", {5.0, 5.0}),
      make_point("arc1.s", {5.0, 0.0}),   make_point("arc1.m", {0.0, 5.0}),
      make_point("arc1.e", {-5.0, 0.0}),  make_point("arc2.s", {10.0, 0.0}),
      make_point("arc2.m", {0.0, 10.0}),  make_point("arc2.e", {-10.0, 0.0})};
  std::vector<SketchTopologyEntity> entities{
      make_entity("entity/x", SketchTopologyEntityKind::Line,
                  {SketchPointId("a0"), SketchPointId("a1")}),
      make_entity("entity/y", SketchTopologyEntityKind::Line,
                  {SketchPointId("a0"), SketchPointId("b0")}),
      make_entity("entity/top", SketchTopologyEntityKind::Line,
                  {SketchPointId("b0"), SketchPointId("b1")}),
      make_entity("entity/col", SketchTopologyEntityKind::Line,
                  {SketchPointId("col.0"), SketchPointId("col.1")}),
      make_entity("entity/tangent", SketchTopologyEntityKind::Line,
                  {SketchPointId("arc1.s"), SketchPointId("tan.end")}),
      make_entity("entity/arc1", SketchTopologyEntityKind::Arc,
                  {SketchPointId("arc1.s"), SketchPointId("arc1.m"),
                   SketchPointId("arc1.e")}),
      make_entity("entity/arc2", SketchTopologyEntityKind::Arc,
                  {SketchPointId("arc2.s"), SketchPointId("arc2.m"),
                   SketchPointId("arc2.e")})};
  const SketchTopology topology =
      SketchTopology::create(SketchId("sketch.families"), std::move(points), std::move(entities))
          .value();

  const std::vector<SketchSolverConstraint> families{
      make_constraint("01.coincident", SketchSolverConstraintKind::Coincident,
                      {point_target("a0"), point_target("dup")}),
      make_constraint("02.fixed", SketchSolverConstraintKind::Fixed, {point_target("a0")}),
      make_constraint("03.horizontal", SketchSolverConstraintKind::Horizontal,
                      {entity_target("entity/x")}),
      make_constraint("04.vertical", SketchSolverConstraintKind::Vertical,
                      {entity_target("entity/y")}),
      make_constraint("05.parallel", SketchSolverConstraintKind::Parallel,
                      {entity_target("entity/x"), entity_target("entity/top")}),
      make_constraint("06.perpendicular", SketchSolverConstraintKind::Perpendicular,
                      {entity_target("entity/x"), entity_target("entity/y")}),
      make_constraint("07.collinear", SketchSolverConstraintKind::Collinear,
                      {entity_target("entity/x"), entity_target("entity/col")}),
      make_constraint("08.equal", SketchSolverConstraintKind::Equal,
                      {entity_target("entity/x"), entity_target("entity/top")}),
      make_constraint("09.tangent", SketchSolverConstraintKind::Tangent,
                      {entity_target("entity/tangent"), entity_target("entity/arc1")}),
      make_constraint("10.concentric", SketchSolverConstraintKind::Concentric,
                      {entity_target("entity/arc1"), entity_target("entity/arc2")}),
      make_constraint("11.midpoint", SketchSolverConstraintKind::Midpoint,
                      {point_target("mid"), entity_target("entity/x")}),
      make_constraint("12.symmetric", SketchSolverConstraintKind::Symmetric,
                      {point_target("sym.a"), point_target("sym.b"), entity_target("entity/x")}),
      make_constraint("13.point-on-object", SketchSolverConstraintKind::PointOnObject,
                      {point_target("on"), entity_target("entity/x")}),
      make_constraint("14.horizontal-distance", SketchSolverConstraintKind::HorizontalDistance,
                      {point_target("a0"), point_target("a1")}, 10.0),
      make_constraint("15.vertical-distance", SketchSolverConstraintKind::VerticalDistance,
                      {point_target("a0"), point_target("b0")}, 5.0),
      make_constraint("16.aligned-distance", SketchSolverConstraintKind::AlignedDistance,
                      {point_target("b0"), point_target("b1")}, 10.0),
      make_constraint("17.radial", SketchSolverConstraintKind::Radial,
                      {entity_target("entity/arc1")}, 5.0),
      make_constraint("18.diameter", SketchSolverConstraintKind::Diameter,
                      {entity_target("entity/arc2")}, 20.0),
      make_constraint("19.angular", SketchSolverConstraintKind::Angular,
                      {entity_target("entity/x"), entity_target("entity/y")}, 90.0)};

  for (const auto& family : families) {
    auto system = SketchConstraintSystem::create(topology.sketch(), {family});
    REQUIRE(system);
    auto solved = SketchConstraintSolver{}.solve(topology, system.value());
    REQUIRE(solved);
    INFO(family.id());
    CHECK(solved.value().status != SketchSolveStatus::InvalidReference);
    CHECK(solved.value().status != SketchSolveStatus::Conflicting);
    CHECK(solved.value().status != SketchSolveStatus::NonConvergent);
    CHECK(solved.value().residuals.final_rms <= 1.0e-9);
  }
}

TEST_CASE("Sketch solver constrains points to cubic splines with a signed distance residual",
          "[core][sketch-solver][core][sketch-spline]") {
  const auto make_spline_topology = [](Point2 candidate) {
    std::vector<SketchTopologyPoint> points{
        make_point("spline.p0", {0.0, 0.0}),
        make_point("spline.p1", {0.0, 6.0}),
        make_point("spline.p2", {10.0, 6.0}),
        make_point("spline.p3", {10.0, 0.0}),
        make_point("candidate", candidate),
    };
    std::vector<SketchTopologyEntity> entities{
        make_entity("entity/spline", SketchTopologyEntityKind::Spline,
                    {SketchPointId("spline.p0"), SketchPointId("spline.p1"),
                     SketchPointId("spline.p2"), SketchPointId("spline.p3")}),
    };
    return SketchTopology::create(SketchId("sketch.spline.point-on"), std::move(points),
                                  std::move(entities))
        .value();
  };

  SECTION("a point already on the Bezier is accepted without arc fallthrough") {
    // B(0.25) for the control polygon above. These three first controls also
    // define a very different circumcircle, so the old implicit Arc fallback
    // would produce a non-zero initial residual and move the geometry.
    const Point2 on_curve{1.5625, 3.375};
    const SketchTopology topology = make_spline_topology(on_curve);
    auto system = SketchConstraintSystem::create(
        topology.sketch(),
        {make_constraint("point-on.spline", SketchSolverConstraintKind::PointOnObject,
                         {point_target("candidate"), entity_target("entity/spline")})});
    REQUIRE(system);

    auto solved = SketchConstraintSolver{}.solve(topology, system.value());
    REQUIRE(solved);
    CHECK(solved.value().status == SketchSolveStatus::UnderConstrained);
    CHECK(solved.value().iterations == 0U);
    CHECK(solved.value().residuals.initial_rms <= 1.0e-12);
    CHECK(solved.value().residuals.final_rms <= 1.0e-12);
    const auto* candidate = solved.value().topology.find_point(SketchPointId("candidate"));
    REQUIRE(candidate != nullptr);
    CHECK(candidate->position().x == Catch::Approx(on_curve.x).margin(1.0e-10));
    CHECK(candidate->position().y == Catch::Approx(on_curve.y).margin(1.0e-10));
  }

  SECTION("an offset point converges onto the fixed spline") {
    const SketchTopology topology = make_spline_topology({5.0, 5.0});
    auto system = SketchConstraintSystem::create(
        topology.sketch(),
        {make_constraint("fixed.spline", SketchSolverConstraintKind::Fixed,
                         {entity_target("entity/spline")}),
         make_constraint("point-on.spline", SketchSolverConstraintKind::PointOnObject,
                         {point_target("candidate"), entity_target("entity/spline")})});
    REQUIRE(system);

    auto solved = SketchConstraintSolver{}.solve(topology, system.value());
    REQUIRE(solved);
    CHECK(solved.value().status == SketchSolveStatus::UnderConstrained);
    CHECK(solved.value().residuals.final_rms <= 1.0e-9);
    const auto* candidate = solved.value().topology.find_point(SketchPointId("candidate"));
    REQUIRE(candidate != nullptr);
    CHECK(candidate->position().x == Catch::Approx(5.0).margin(1.0e-6));
    CHECK(candidate->position().y == Catch::Approx(4.5).margin(1.0e-6));
  }
}

TEST_CASE("Sketch point-on-spline remains finite at a degenerate endpoint tangent",
          "[core][sketch-solver][core][sketch-spline]") {
  std::vector<SketchTopologyPoint> points{
      make_point("spline.p0", {0.0, 0.0}), make_point("spline.p1", {0.0, 0.0}),
      make_point("spline.p2", {5.0, 0.0}), make_point("spline.p3", {5.0, 5.0}),
      make_point("candidate", {0.0, 0.0}),
  };
  std::vector<SketchTopologyEntity> entities{
      make_entity("entity/spline", SketchTopologyEntityKind::Spline,
                  {SketchPointId("spline.p0"), SketchPointId("spline.p1"),
                   SketchPointId("spline.p2"), SketchPointId("spline.p3")}),
  };
  const SketchTopology topology =
      SketchTopology::create(SketchId("sketch.spline.degenerate"), std::move(points),
                             std::move(entities))
          .value();
  auto system = SketchConstraintSystem::create(
      topology.sketch(),
      {make_constraint("point-on.spline", SketchSolverConstraintKind::PointOnObject,
                       {point_target("candidate"), entity_target("entity/spline")})});
  REQUIRE(system);

  auto solved = SketchConstraintSolver{}.solve(topology, system.value());
  REQUIRE(solved);
  CHECK(solved.value().status == SketchSolveStatus::UnderConstrained);
  CHECK(std::isfinite(solved.value().residuals.initial_rms));
  CHECK(std::isfinite(solved.value().residuals.final_rms));
  CHECK(solved.value().residuals.final_rms <= 1.0e-12);
}

TEST_CASE("Sketch solver reports deterministic redundant constraints",
          "[core][sketch-dof][core][sketch-conflict-diagnostics]") {
  const SketchTopology topology = make_line_topology({0.0, 0.0}, {10.0, 0.0});
  auto system = SketchConstraintSystem::create(
      topology.sketch(),
      {make_constraint("a.horizontal", SketchSolverConstraintKind::Horizontal,
                       {entity_target("entity/line.a")}),
       make_constraint("b.horizontal", SketchSolverConstraintKind::Horizontal,
                       {entity_target("entity/line.a")})});
  REQUIRE(system);
  auto solved = SketchConstraintSolver{}.solve(topology, system.value());
  REQUIRE(solved);
  CHECK(solved.value().status == SketchSolveStatus::Redundant);
  CHECK(solved.value().redundant_constraint_ids == std::vector<std::string>{"b.horizontal"});
  REQUIRE(solved.value().diagnostics.size() == 1U);
  CHECK(solved.value().diagnostics[0].kind == SketchSolverDiagnosticKind::RedundantConstraint);
  CHECK(solved.value().diagnostics[0].constraint_id == "b.horizontal");
}

TEST_CASE("Sketch solver attributes a stable conflicting constraint set",
          "[core][sketch-solver][core][sketch-conflict-diagnostics]") {
  const SketchTopology topology = make_line_topology({0.0, 0.0}, {5.0, 2.0});
  auto system = SketchConstraintSystem::create(
      topology.sketch(),
      {make_constraint("constraint.fixed", SketchSolverConstraintKind::Fixed,
                       {point_target("point.a")}),
       make_constraint("constraint.horizontal", SketchSolverConstraintKind::Horizontal,
                       {entity_target("entity/line.a")}),
       make_constraint("distance.10", SketchSolverConstraintKind::HorizontalDistance,
                       {point_target("point.a"), point_target("point.b")}, 10.0),
       make_constraint("distance.20", SketchSolverConstraintKind::HorizontalDistance,
                       {point_target("point.a"), point_target("point.b")}, 20.0)});
  REQUIRE(system);
  auto solved = SketchConstraintSolver{}.solve(topology, system.value());
  REQUIRE(solved);
  CHECK(solved.value().status == SketchSolveStatus::Conflicting);
  CHECK(solved.value().conflicting_constraint_ids ==
        std::vector<std::string>{"distance.10", "distance.20"});
  REQUIRE(solved.value().diagnostics.size() == 2U);
  CHECK(solved.value().diagnostics[0].constraint_id == "distance.10");
  CHECK(solved.value().diagnostics[1].constraint_id == "distance.20");
}

TEST_CASE("Sketch solver distinguishes invalid references and non convergence",
          "[core][sketch-solver][core][sketch-conflict-diagnostics]") {
  const SketchTopology topology = make_line_topology();
  auto invalid_system = SketchConstraintSystem::create(
      topology.sketch(),
      {make_constraint("invalid.coincident", SketchSolverConstraintKind::Coincident,
                       {point_target("point.a"), point_target("point.missing")})});
  REQUIRE(invalid_system);
  auto invalid = SketchConstraintSolver{}.solve(topology, invalid_system.value());
  REQUIRE(invalid);
  CHECK(invalid.value().status == SketchSolveStatus::InvalidReference);
  REQUIRE(invalid.value().diagnostics.size() == 1U);
  CHECK(invalid.value().diagnostics[0].constraint_id == "invalid.coincident");

  std::vector<SketchTopologyPoint> points{
      make_point("arc.s", {5.0, 0.0}), make_point("arc.m", {0.0, 5.0}),
      make_point("arc.e", {-5.0, 0.0})};
  std::vector<SketchTopologyEntity> entities{
      make_entity("entity/arc", SketchTopologyEntityKind::Arc,
                  {SketchPointId("arc.s"), SketchPointId("arc.m"), SketchPointId("arc.e")})};
  const SketchTopology arc_topology =
      SketchTopology::create(SketchId("sketch.nonconvergent"), std::move(points),
                             std::move(entities))
          .value();
  auto nonlinear_system = SketchConstraintSystem::create(
      arc_topology.sketch(),
      {make_constraint("radius.target", SketchSolverConstraintKind::Radial,
                       {entity_target("entity/arc")}, 7.123456789)});
  REQUIRE(nonlinear_system);
  SketchSolverOptions options;
  options.maximum_iterations = 1U;
  options.convergence_rms = 1.0e-16;
  auto nonconvergent = SketchConstraintSolver{}.solve(arc_topology, nonlinear_system.value(), options);
  REQUIRE(nonconvergent);
  CHECK(nonconvergent.value().status == SketchSolveStatus::NonConvergent);
}

TEST_CASE("Persisted Sketch constraints and dimensions adapt to the Block 109 solver",
          "[core][sketch-solver][core][sketch-dof]") {
  auto document = PartDocument::create(DocumentId("part.solver"), "SolverPart");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(Parameter::create_length(
      ParameterId("part.width"), "Width", Quantity::length_mm(10.0).value()).value()));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));

  auto sketch = Sketch::create(SketchId("sketch.persisted"), "Sketch_Persisted",
                               DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_entity(LineSegment::create(
      SketchEntityId("line.a"), Point2{0.0, 0.0}, Point2{8.0, 2.0}).value()));
  const auto line = SketchReferenceTarget::create_line_segment(SketchEntityId("line.a")).value();
  const auto start =
      SketchReferenceTarget::create_line_segment_start(SketchEntityId("line.a")).value();
  const auto end =
      SketchReferenceTarget::create_line_segment_end(SketchEntityId("line.a")).value();
  REQUIRE(sketch.value().add_constraint(SketchGeometricConstraint::create_fixed(
      SketchConstraintId("fixed.start"), start).value()));
  REQUIRE(sketch.value().add_constraint(SketchGeometricConstraint::create_horizontal(
      SketchConstraintId("horizontal.line"), line).value()));
  REQUIRE(sketch.value().add_dimension(SketchDrivingDimension::create_horizontal_distance(
      SketchDimensionId("width"), start, end, ParameterId("part.width")).value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto topology = SketchTopology::migrate_legacy(sketch.value());
  REQUIRE(topology);
  auto system = SketchConstraintSystemBuilder::from_legacy(
      topology.value(), *document.value().find_sketch(SketchId("sketch.persisted")), document.value());
  REQUIRE(system);
  REQUIRE(system.value().constraints().size() == 3U);
  CHECK(system.value().constraints()[0].id() == "dimension/width");
  CHECK(system.value().constraints()[1].id() == "geometric-constraint/fixed.start");
  CHECK(system.value().constraints()[2].id() == "geometric-constraint/horizontal.line");

  auto solved = SketchConstraintSolver{}.solve(topology.value(), system.value());
  REQUIRE(solved);
  CHECK(solved.value().status == SketchSolveStatus::FullyConstrained);
  const auto* solved_line = solved.value().topology.find_entity("entity/line.a");
  REQUIRE(solved_line != nullptr);
  const auto* solved_end = solved.value().topology.find_point(solved_line->points()[1]);
  REQUIRE(solved_end != nullptr);
  CHECK(solved_end->position().x == Catch::Approx(10.0).margin(1.0e-7));
  CHECK(solved_end->position().y == Catch::Approx(0.0).margin(1.0e-7));
}

TEST_CASE("Sketch solver converges a line tangent to a circle at a coincident endpoint",
          "[core][sketch-solver][core][sketch-tangent]") {
  // Regression: a line whose endpoint is held on a circle by a coincidence
  // (PointOnObject) plus a Tangent between the same line and circle used to
  // creep to the iteration limit (non_convergent) because the squared
  // distance tangent residual (d²−r²) shared the point-on-circle radial
  // gradient at the solution. The tangent residual now switches to
  // perpendicularity (direction · (endpoint − center) = 0) at a held endpoint.
  constexpr double kRadius = 5.0;
  std::vector<SketchTopologyPoint> points{
      make_point("circ.center", {10.0, 0.0}),
      make_point("line.on", {10.0, 5.0}), // on the circle (12 o'clock)
      make_point("line.far", {-5.0, -10.0})};
  std::vector<SketchTopologyEntity> entities{
      make_entity("profile/circ", SketchTopologyEntityKind::CircleProfile,
                  {SketchPointId("circ.center")}),
      make_entity("entity/line", SketchTopologyEntityKind::Line,
                  {SketchPointId("line.on"), SketchPointId("line.far")})};
  const SketchTopology topology =
      SketchTopology::create(SketchId("sketch.tangent"), std::move(points), std::move(entities))
          .value();
  auto system = SketchConstraintSystem::create(
      topology.sketch(),
      {make_constraint("point_on.end", SketchSolverConstraintKind::PointOnObject,
                       {point_target("line.on"), entity_target("profile/circ")}, kRadius),
       make_constraint("tangent.line", SketchSolverConstraintKind::Tangent,
                       {entity_target("entity/line"), entity_target("profile/circ")}, kRadius)});
  REQUIRE(system);

  auto solved = SketchConstraintSolver{}.solve(topology, system.value());
  REQUIRE(solved);
  CHECK(solved.value().status == SketchSolveStatus::UnderConstrained);
  CHECK(solved.value().residuals.final_rms <= 1.0e-8);

  const auto* center = solved.value().topology.find_point(SketchPointId("circ.center"));
  const auto* on = solved.value().topology.find_point(SketchPointId("line.on"));
  const auto* far = solved.value().topology.find_point(SketchPointId("line.far"));
  REQUIRE(center != nullptr);
  REQUIRE(on != nullptr);
  REQUIRE(far != nullptr);
  // Endpoint stays on the circle.
  const double on_dx = on->position().x - center->position().x;
  const double on_dy = on->position().y - center->position().y;
  CHECK(std::sqrt(on_dx * on_dx + on_dy * on_dy) == Catch::Approx(kRadius).margin(1.0e-6));
  // Line is perpendicular to the radius at the contact point (i.e. tangent).
  const double dir_x = far->position().x - on->position().x;
  const double dir_y = far->position().y - on->position().y;
  const double dot = dir_x * on_dx + dir_y * on_dy;
  const double dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
  CHECK(dot / dir_len == Catch::Approx(0.0).margin(1.0e-6));
}

TEST_CASE("Sketch solver generalized collinear aligns three points and a point to a line",
          "[core][sketch-solver][core][sketch-collinear]") {
  std::vector<SketchTopologyPoint> points{
      make_point("p0", {0.0, 0.0}),  make_point("p1", {10.0, 0.0}),
      make_point("p2", {5.0, 4.0}),  make_point("l0", {0.0, -3.0}),
      make_point("l1", {8.0, -3.0}), make_point("onl", {5.0, 5.0})};
  std::vector<SketchTopologyEntity> entities{
      make_entity("entity/base", SketchTopologyEntityKind::Line,
                  {SketchPointId("l0"), SketchPointId("l1")})};
  const SketchTopology topology =
      SketchTopology::create(SketchId("sketch.collinear"), std::move(points), std::move(entities))
          .value();

  SECTION("three points collinear snaps the free point onto the reference line") {
    auto system = SketchConstraintSystem::create(
        topology.sketch(),
        {make_constraint("fix.0", SketchSolverConstraintKind::Fixed, {point_target("p0")}),
         make_constraint("fix.1", SketchSolverConstraintKind::Fixed, {point_target("p1")}),
         make_constraint("collinear.3", SketchSolverConstraintKind::Collinear,
                         {point_target("p0"), point_target("p1"), point_target("p2")})});
    REQUIRE(system);
    auto solved = SketchConstraintSolver{}.solve(topology, system.value());
    REQUIRE(solved);
    CHECK(solved.value().status != SketchSolveStatus::Conflicting);
    CHECK(solved.value().residuals.final_rms <= 1.0e-8);
    const auto* p2 = solved.value().topology.find_point(SketchPointId("p2"));
    REQUIRE(p2 != nullptr);
    CHECK(p2->position().y == Catch::Approx(0.0).margin(1.0e-6)); // on the y=0 line
  }

  SECTION("point collinear with a line moves onto the line's infinite extension") {
    auto system = SketchConstraintSystem::create(
        topology.sketch(),
        {make_constraint("fix.l0", SketchSolverConstraintKind::Fixed, {point_target("l0")}),
         make_constraint("fix.l1", SketchSolverConstraintKind::Fixed, {point_target("l1")}),
         make_constraint("collinear.pl", SketchSolverConstraintKind::Collinear,
                         {point_target("onl"), entity_target("entity/base")})});
    REQUIRE(system);
    auto solved = SketchConstraintSolver{}.solve(topology, system.value());
    REQUIRE(solved);
    CHECK(solved.value().status != SketchSolveStatus::Conflicting);
    CHECK(solved.value().residuals.final_rms <= 1.0e-8);
    const auto* onl = solved.value().topology.find_point(SketchPointId("onl"));
    REQUIRE(onl != nullptr);
    CHECK(onl->position().y == Catch::Approx(-3.0).margin(1.0e-6)); // on the y=-3 line
  }
}
