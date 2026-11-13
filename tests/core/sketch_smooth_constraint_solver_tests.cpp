#include "blcad/core/sketch_constraint_authoring_json.hpp"
#include "blcad/core/sketch_constraint_solver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace blcad;
namespace {
SketchSolverTarget point(const char* id) {
  return SketchSolverTarget::point(SketchPointId(id)).value();
}
SketchSolverTarget entity(const char* id) {
  return SketchSolverTarget::entity(id).value();
}
SketchSolverConstraint constraint(const char* id, SketchSolverConstraintKind kind,
                                  std::vector<SketchSolverTarget> targets,
                                  std::optional<double> value = {}) {
  return SketchSolverConstraint::create(id, kind, std::move(targets), value).value();
}
SketchTopology line_junction(double scale = 1, bool distinct = false, bool reversed = false,
                             bool curved = true, bool cusp = false) {
  std::vector<SketchTopologyPoint> points;
  for (const auto& [id, position] :
       std::vector<std::pair<std::string, Point2>>{{"a.corner", {0, 0}},
                                                   {"b.handle", {2 * scale, 0}},
                                                   {"c.free", {4 * scale, curved ? scale : 0}},
                                                   {"d.end", {6 * scale, 3 * scale}},
                                                   {"e.lineFar", {(cusp ? 5 : -5) * scale, 0}}})
    points.push_back(SketchTopologyPoint::create(SketchPointId(id), position).value());
  if (distinct)
    points.push_back(
        SketchTopologyPoint::create(SketchPointId("f.lineCorner"), {0, .000001 * scale}).value());
  std::vector<SketchPointId> poles{SketchPointId("a.corner"), SketchPointId("b.handle"),
                                   SketchPointId("c.free"), SketchPointId("d.end")};
  if (reversed)
    std::reverse(poles.begin(), poles.end());
  return SketchTopology::create(
             SketchId("g2"), std::move(points),
             {SketchTopologyEntity::create("curve.spline", SketchTopologyEntityKind::Spline, poles)
                  .value(),
              SketchTopologyEntity::create("curve.line", SketchTopologyEntityKind::Line,
                                           {SketchPointId(distinct ? "f.lineCorner" : "a.corner"),
                                            SketchPointId("e.lineFar")})
                  .value()})
      .value();
}
SketchSolverConstraint smooth(bool distinct = false, bool swapped = false) {
  auto targets = std::vector<SketchSolverTarget>{entity("curve.spline"), point("a.corner"),
                                                 entity("curve.line"),
                                                 point(distinct ? "f.lineCorner" : "a.corner")};
  if (swapped) {
    std::swap(targets[0], targets[2]);
    std::swap(targets[1], targets[3]);
  }
  return constraint("smooth", SketchSolverConstraintKind::Smooth, std::move(targets));
}
std::vector<SketchSolverConstraint> held_junction(bool distinct = false, bool swapped = false) {
  std::vector<SketchSolverConstraint> constraints;
  for (const char* id : {"a.corner", "b.handle", "d.end", "e.lineFar"})
    constraints.push_back(SketchSolverConstraint::create(std::string("fixed/") + id,
                                                         SketchSolverConstraintKind::Fixed,
                                                         {point(id)})
                              .value());
  constraints.push_back(smooth(distinct, swapped));
  return constraints;
}
} // namespace

TEST_CASE("Smooth solves an oriented line junction with independent local rank and driving oracles",
          "[core][parity-b159][sketch-smooth][sketch-solver]") {
  for (double scale : {.000001, .001, 1.0, 1000.0, 1000000.0})
    for (bool distinct : {false, true})
      for (bool reversed : {false, true})
        for (bool swapped : {false, true})
          for (bool driving : {false, true}) {
            CAPTURE(scale, distinct, reversed, swapped, driving);
            const auto source = line_junction(scale, distinct, reversed);
            const auto before = source;
            auto constraints = held_junction(distinct, swapped);
            if (driving)
              constraints.push_back(constraint("driving/x",
                                               SketchSolverConstraintKind::HorizontalDistance,
                                               {point("a.corner"), point("c.free")}, 4 * scale));
            const auto system =
                SketchConstraintSystem::create(source.sketch(), constraints).value();
            const auto result = SketchConstraintSolver{}.solve(source, system);
            REQUIRE(result);
            CHECK(source == before);
            CHECK(result.value().status == (driving ? SketchSolveStatus::FullyConstrained
                                                    : SketchSolveStatus::UnderConstrained));
            const std::size_t variables = distinct ? 12 : 10;
            CHECK(result.value().variable_order.size() == variables);
            CHECK(result.value().jacobian_rank == variables - (driving ? 0 : 1));
            CHECK(result.value().remaining_dof == (driving ? 0 : 1));
            CHECK(result.value().residuals.final_rms <= 1e-9);
            CHECK(result.value().variable_order[0].point == SketchPointId("a.corner"));
            CHECK(result.value().variable_order[0].axis == SketchSolverVariableAxis::X);
            CHECK(result.value().variable_order[1].axis == SketchSolverVariableAxis::Y);
            const auto p0 =
                result.value().topology.find_point(SketchPointId("a.corner"))->position();
            const auto p1 =
                result.value().topology.find_point(SketchPointId("b.handle"))->position();
            const auto p2 = result.value().topology.find_point(SketchPointId("c.free"))->position();
            // Analytic cubic endpoint condition against a horizontal line:
            // y0=y1=y2=0; the unused second-control X remains one free DOF.
            CHECK(p0.y == Catch::Approx(0).margin(1e-7 * scale));
            CHECK(p1.y == Catch::Approx(0).margin(1e-7 * scale));
            CHECK(p2.y == Catch::Approx(0).margin(1e-7 * scale));
            if (driving)
              CHECK(p2.x == Catch::Approx(4 * scale).margin(1e-7 * scale));
            if (distinct) {
              CHECK(p0 == Point2{0, 0});
              CHECK(result.value().topology.find_point(SketchPointId("f.lineCorner"))->position() ==
                    p0);
            }
          }
}

TEST_CASE("Smooth attributes the already satisfied legacy tangent as redundant",
          "[core][parity-b159][sketch-smooth][sketch-solver]") {
  const auto source = line_junction(1, false, false, false);
  auto g2 = smooth();
  g2 = constraint("a.smooth", g2.kind(), g2.targets());
  const auto system =
      SketchConstraintSystem::create(
          source.sketch(), {g2, constraint("z.tangent", SketchSolverConstraintKind::Tangent,
                                           {entity("curve.spline"), entity("curve.line")})})
          .value();
  const auto result = SketchConstraintSolver{}.solve(source, system);
  REQUIRE(result);
  CHECK(result.value().status == SketchSolveStatus::Redundant);
  CHECK(result.value().jacobian_rank == 2);
  CHECK(result.value().remaining_dof == 8);
  CHECK(result.value().redundant_constraint_ids == std::vector<std::string>{"z.tangent"});
}

TEST_CASE("Smooth does not certify a fixed cusp from zero unsigned curvature",
          "[core][parity-b159][sketch-smooth][sketch-solver]") {
  const auto source = line_junction(1, false, false, false, true);
  const auto fixed_spline =
      constraint("fixed/spline", SketchSolverConstraintKind::Fixed, {entity("curve.spline")});
  const auto fixed_line =
      constraint("fixed/line", SketchSolverConstraintKind::Fixed, {entity("curve.line")});
  const auto g1 = SketchConstraintSystem::create(
                      source.sketch(), {fixed_spline, fixed_line,
                                        constraint("tangent", SketchSolverConstraintKind::Tangent,
                                                   {entity("curve.spline"), entity("curve.line")})})
                      .value();
  const auto historical = SketchConstraintSolver{}.solve(source, g1);
  REQUIRE(historical);
  CHECK(historical.value().residuals.final_max_abs == 0);
  auto combined = g1.constraints();
  combined.push_back(smooth());
  const auto system = SketchConstraintSystem::create(source.sketch(), std::move(combined)).value();
  const auto result = SketchConstraintSolver{}.solve(source, system);
  REQUIRE(result);
  CHECK(result.value().status == SketchSolveStatus::Conflicting);
  CHECK(std::find(result.value().conflicting_constraint_ids.begin(),
                  result.value().conflicting_constraint_ids.end(),
                  "smooth") != result.value().conflicting_constraint_ids.end());
}

TEST_CASE("Smooth rejects wrong endpoint identity without proximity retargeting",
          "[core][parity-b159][sketch-smooth][sketch-solver]") {
  const auto source = line_junction();
  auto targets = smooth().targets();
  targets[1] = point("b.handle");
  const auto invalid = constraint("invalid", SketchSolverConstraintKind::Smooth, targets);
  const auto result = SketchConstraintSolver{}.solve(
      source, SketchConstraintSystem::create(source.sketch(), {invalid}).value());
  REQUIRE(result);
  CHECK(result.value().status == SketchSolveStatus::InvalidReference);
  CHECK(result.value().topology == source);
  REQUIRE(result.value().diagnostics.size() == 1);
  CHECK(result.value().diagnostics[0].constraint_id == "invalid");
  CHECK_FALSE(SketchSolverConstraint::create("value", SketchSolverConstraintKind::Smooth,
                                             smooth().targets(), 1.0));
  targets.pop_back();
  CHECK_FALSE(SketchSolverConstraint::create("arity", SketchSolverConstraintKind::Smooth, targets));
}

TEST_CASE("Smooth solves rational conic junctions against arcs and unequal-speed splines",
          "[core][parity-b159][sketch-smooth][sketch-solver]") {
  const double q = std::sqrt(.5), w = (1 + 2 * q) / 3, m = 10 * q / (1 + 2 * q);
  for (bool other_spline : {false, true})
    for (bool reverse : {false, true})
      for (double speed : {.25, 1.0, 4.0}) {
        CAPTURE(other_spline, reverse, speed);
        std::vector<SketchTopologyPoint> points;
        for (const auto& [id, p] : std::vector<std::pair<std::string, Point2>>{
                 {"a0", {5, 0}},
                 {"a1", {5, m}},
                 {"a2", {m + .2, 5}},
                 {"a3", {0, 5}},
                 {"b0", {5, 0}},
                 {"b1", other_spline ? Point2{5, -m} : Point2{0, -5}},
                 {"b2", other_spline ? Point2{m, -5} : Point2{-5, 0}},
                 {"b3", {0, -5}}})
          if (other_spline || id != "b3")
            points.push_back(SketchTopologyPoint::create(SketchPointId(id), p).value());
        std::vector<SketchPointId> a{SketchPointId("a0"), SketchPointId("a1"), SketchPointId("a2"),
                                     SketchPointId("a3")};
        std::vector<SketchPointId> b{SketchPointId("b0"), SketchPointId("b1"), SketchPointId("b2")};
        if (other_spline)
          b.push_back(SketchPointId("b3"));
        std::array<double, 4> weights{1, w * speed, w * speed * speed, speed * speed * speed};
        if (reverse) {
          std::reverse(a.begin(), a.end());
          std::reverse(weights.begin(), weights.end());
        }
        const auto source =
            SketchTopology::create(
                SketchId("conics"), points,
                {SketchTopologyEntity::create("a", SketchTopologyEntityKind::Spline, a, {}, {},
                                              weights)
                     .value(),
                 SketchTopologyEntity::create("b",
                                              other_spline ? SketchTopologyEntityKind::Spline
                                                           : SketchTopologyEntityKind::Arc,
                                              b, {}, {},
                                              other_spline ? std::array<double, 4>{1, w, w, 1}
                                                           : std::array<double, 4>{1, 1, 1, 1})
                     .value()})
                .value();
        std::vector<SketchSolverConstraint> constraints{
            constraint("fixed/b", SketchSolverConstraintKind::Fixed, {entity("b")})};
        for (const char* id : {"a0", "a1", "a3"})
          constraints.push_back(constraint((std::string("fixed/") + id).c_str(),
                                           SketchSolverConstraintKind::Fixed, {point(id)}));
        constraints.push_back(constraint("smooth", SketchSolverConstraintKind::Smooth,
                                         {entity("a"), point("a0"), entity("b"), point("b0")}));
        const auto solved = SketchConstraintSolver{}.solve(
            source, SketchConstraintSystem::create(source.sketch(), constraints).value());
        REQUIRE(solved);
        CHECK(solved.value().status == SketchSolveStatus::UnderConstrained);
        CHECK(solved.value().remaining_dof == 1);
        CHECK(solved.value().jacobian_rank == 2 * points.size() - 1);
        CHECK(solved.value().topology.find_point(SketchPointId("a2"))->position().x ==
              Catch::Approx(m).margin(1e-7));
        CHECK(solved.value().residuals.final_rms <= 1e-9);
        // Analytic circle curvature at (5,0) is (-1/5,0), independently
        // determined by the conic's center and radius, not a control polygon.
        const auto solved_x = solved.value().topology.find_point(SketchPointId("a2"))->position().x;
        const double normal_second = 6 * w * speed * speed * (solved_x - 5);
        const double initial_speed = 3 * w * speed * m;
        CHECK(normal_second / (initial_speed * initial_speed) == Catch::Approx(-.2).margin(1e-8));
      }
}

TEST_CASE("Equal unsigned radii cannot certify opposite curvature and degenerate endpoints",
          "[core][parity-b159][sketch-smooth][sketch-solver]") {
  const double q = std::sqrt(.5), w = (1 + 2 * q) / 3, m = 10 * q / (1 + 2 * q);
  for (bool degenerate : {false, true}) {
    CAPTURE(degenerate);
    std::vector<SketchTopologyPoint> points;
    for (const auto& [id, p] :
         std::vector<std::pair<std::string, Point2>>{{"a0", {5, 0}},
                                                     {"a1", {5, degenerate ? 0 : m}},
                                                     {"a2", {m, 5}},
                                                     {"a3", {0, 5}},
                                                     {"b0", {5, 0}},
                                                     {"b1", {10, -5}},
                                                     {"b2", {15, 0}}})
      points.push_back(SketchTopologyPoint::create(SketchPointId(id), p).value());
    const auto source =
        SketchTopology::create(
            SketchId("opposite"), points,
            {SketchTopologyEntity::create("a", SketchTopologyEntityKind::Spline,
                                          {SketchPointId("a0"), SketchPointId("a1"),
                                           SketchPointId("a2"), SketchPointId("a3")},
                                          {}, {}, {1, w, w, 1})
                 .value(),
             SketchTopologyEntity::create(
                 "b", SketchTopologyEntityKind::Arc,
                 {SketchPointId("b0"), SketchPointId("b1"), SketchPointId("b2")})
                 .value()})
            .value();
    const auto before = source;
    const auto solved = SketchConstraintSolver{}.solve(
        source, SketchConstraintSystem::create(
                    source.sketch(),
                    {constraint("fixed/a", SketchSolverConstraintKind::Fixed, {entity("a")}),
                     constraint("fixed/b", SketchSolverConstraintKind::Fixed, {entity("b")}),
                     constraint("smooth", SketchSolverConstraintKind::Smooth,
                                {entity("a"), point("a0"), entity("b"), point("b0")})})
                    .value());
    REQUIRE(solved);
    CHECK(source == before);
    CHECK(solved.value().status ==
          (degenerate ? SketchSolveStatus::InvalidReference : SketchSolveStatus::Conflicting));
    if (!degenerate) {
      CHECK(solved.value().residuals.initial_rms > 1);
      CHECK(solved.value().residuals.final_rms > SketchSolverOptions{}.convergence_rms);
      CHECK(std::find(solved.value().conflicting_constraint_ids.begin(),
                      solved.value().conflicting_constraint_ids.end(),
                      "smooth") != solved.value().conflicting_constraint_ids.end());
    }
  }
}

TEST_CASE("Smooth persists ordered stable endpoints in version two without altering old records",
          "[core][parity-b159][sketch-smooth][sketch-constraint-json]") {
  const auto intent = SketchConstraintIntent::create(
      SketchConstraintId("smooth"), SketchSolverConstraintKind::Smooth,
      {SketchConstraintIntentTarget::entity("spline").value(),
       SketchConstraintIntentTarget::point(SketchPointId("junction")).value(),
       SketchConstraintIntentTarget::entity("line").value(),
       SketchConstraintIntentTarget::point(SketchPointId("junction")).value()});
  REQUIRE(intent);
  const auto catalog =
      SketchConstraintCatalog::create(DocumentId("part"), SketchId("sketch"), {intent.value()})
          .value();
  const auto serialized = serialize_sketch_constraint_catalog_to_json(catalog);
  REQUIRE(serialized);
  auto root = nlohmann::json::parse(serialized.value());
  CHECK(root.at("version") == 2);
  auto roundtrip = deserialize_sketch_constraint_catalogs_from_json(serialized.value());
  REQUIRE(roundtrip);
  CHECK(roundtrip.value() == std::vector<SketchConstraintCatalog>{catalog});
  const auto empty =
      SketchConstraintCatalog::create(DocumentId("part"), SketchId("sketch")).value();
  const auto legacy = serialize_sketch_constraint_catalog_to_json(empty).value();
  CHECK(nlohmann::json::parse(legacy).at("version") == 1);
  REQUIRE(deserialize_sketch_constraint_catalogs_from_json(legacy));
  for (const auto& version : std::vector<nlohmann::json>{1, 0, 3, 2.5, true, "2", nullptr}) {
    CAPTURE(version);
    auto invalid = root;
    invalid["version"] = version;
    CHECK_FALSE(deserialize_sketch_constraint_catalogs_from_json(invalid.dump()));
  }
  for (int mutation = 0; mutation < 4; ++mutation) {
    CAPTURE(mutation);
    auto invalid = root;
    auto& record = invalid["catalogs"][0]["constraints"][0];
    if (mutation == 0)
      record["targets"].erase(record["targets"].end() - 1);
    if (mutation == 1)
      record["targets"][2] = record["targets"][0];
    if (mutation == 2)
      record["targets"][1]["kind"] = "entity";
    if (mutation == 3)
      record["kind"] = "g3";
    CHECK_FALSE(deserialize_sketch_constraint_catalogs_from_json(invalid.dump()));
  }
}

TEST_CASE("Read-only Smooth curves retain their real scale and deterministic source coordinates",
          "[core][parity-b159][sketch-smooth][sketch-solver]") {
  for (double scale : {.000001, 1.0, 1000000.0}) {
    CAPTURE(scale);
    const auto seed = line_junction(scale);
    std::vector<SketchTopologyPoint> points;
    for (const auto& p : seed.points())
      points.push_back(SketchTopologyPoint::create(SketchPointId("reference/" + p.id().value()),
                                                   p.position(), {false, true})
                           .value());
    std::vector<SketchTopologyEntity> entities;
    for (const auto& e : seed.entities()) {
      std::vector<SketchPointId> ids;
      for (const auto& id : e.points())
        ids.emplace_back("reference/" + id.value());
      entities.push_back(
          SketchTopologyEntity::create(e.id(), e.kind(), ids, {}, {false, true}).value());
    }
    const auto source = SketchTopology::create(seed.sketch(), points, entities).value();
    const auto c = constraint("smooth", SketchSolverConstraintKind::Smooth,
                              {entity("curve.spline"), point("reference/a.corner"),
                               entity("curve.line"), point("reference/a.corner")});
    const auto system = SketchConstraintSystem::create(source.sketch(), {c}).value();
    const auto solved = SketchConstraintSolver{}.solve(source, system);
    REQUIRE(solved);
    CHECK(solved.value().status == SketchSolveStatus::Conflicting);
    CHECK(solved.value().variable_order.empty());
    CHECK(solved.value().residuals.initial_rms > .1);
    CHECK(solved.value().topology == source);
    const auto repeated = SketchConstraintSolver{}.solve(source, system);
    REQUIRE(repeated);
    CHECK(repeated.value() == solved.value());
  }
}
