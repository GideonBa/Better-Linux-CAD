#include "assembly_constraint_numeric_system.hpp"

#include "blcad/geometry/assembly_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_solve_diagnostics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <initializer_list>
#include <numbers>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

constexpr double kSolveTolerance = 1.0e-6;

struct ComponentSpec {
  const char* id;
  RigidTransform transform;
  ComponentGroundingState grounding = ComponentGroundingState::Free;
};

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_plate_part() {
  auto part = PartDocument::create(DocumentId("part.plate"), "Plate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(base_sketch.value()));

  auto base_feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base_feature);
  REQUIRE(part.value().add_feature(base_feature.value()));
  return part.value();
}

Project make_project(std::initializer_list<ComponentSpec> components) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.angles"), "AngleAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.plate")));
  for (const auto& spec : components) {
    auto component =
        ComponentInstance::create(ComponentInstanceId(spec.id), spec.id, DocumentId("part.plate"),
                                  ComponentVisibility::Visible, ComponentSuppressionState::Active,
                                  spec.grounding, spec.transform);
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }

  auto project = Project::create(DocumentId("project.angles"), "AngleProject", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_plate_part()));
  return project.value();
}

AssemblyConstraintTarget make_target(const char* component, const char* semantic_reference) {
  auto target =
      AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

Quantity make_angle_value(double degrees, const char* id) {
  auto angle = Quantity::angle_deg(degrees, id);
  REQUIRE(angle);
  return angle.value();
}

Quantity make_distance(double millimeters, const char* id) {
  auto distance = Quantity::length_mm(millimeters, id);
  REQUIRE(distance);
  return distance.value();
}

void add_constraint(Project& project, const char* id, AssemblyConstraintType type,
                    const char* reference, std::optional<Quantity> distance = std::nullopt,
                    std::optional<Quantity> angle = std::nullopt) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, type, make_target("component.a", reference),
      make_target("component.b", reference), AssemblyConstraintState::Active, std::move(distance),
      std::move(angle));
  REQUIRE(constraint);
  REQUIRE(project.assembly().add_constraint(constraint.value()));
}

void add_angle(Project& project, double degrees, const char* id = "constraint.angle") {
  add_constraint(project, id, AssemblyConstraintType::Angle, "feature.base_extrude.top",
                 std::nullopt, make_angle_value(degrees, id));
}

std::vector<ComponentInstanceId> group(std::initializer_list<const char*> ids) {
  std::vector<ComponentInstanceId> result;
  result.reserve(ids.size());
  for (const char* id : ids) {
    result.emplace_back(id);
  }
  return result;
}

const ProposedComponentTransform& proposal_for(const AssemblySolveResult& result,
                                               const char* component) {
  for (const auto& proposal : result.proposed_transforms) {
    if (proposal.component_instance.value() == component) {
      return proposal;
    }
  }
  FAIL("missing component transform proposal");
  return result.proposed_transforms.front();
}

double angle_alignment_of(const Project& project, const char* constraint_id) {
  const AssemblyConstraint* constraint =
      project.assembly().find_constraint(AssemblyConstraintId(constraint_id));
  REQUIRE(constraint != nullptr);
  const AssemblyConstraintEquationBuilder builder;
  const auto equation = builder.build(project, *constraint);
  REQUIRE(equation);
  const auto* angle = std::get_if<PlanarAngleResidualDescriptor>(&equation.value().residual);
  REQUIRE(angle != nullptr);
  return angle->angle_alignment;
}

} // namespace

TEST_CASE("Planar Angle residual measures normal alignment against the target cosine",
          "[geometry][assembly-angle-solver]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{}}}});
  add_angle(project, 60.0);

  const AssemblyConstraint* constraint =
      project.assembly().find_constraint(AssemblyConstraintId("constraint.angle"));
  REQUIRE(constraint != nullptr);
  const AssemblyConstraintEquationBuilder builder;
  const auto equation = builder.build(project, *constraint);
  REQUIRE(equation);
  const auto* angle = std::get_if<PlanarAngleResidualDescriptor>(&equation.value().residual);
  REQUIRE(angle != nullptr);
  CHECK(angle->target_angle_deg == Approx(60.0));
  CHECK(angle->normal_dot == Approx(1.0));
  CHECK(angle->angle_alignment == Approx(0.5).margin(1.0e-12));

  Project rotated_project = project;
  REQUIRE(rotated_project.assembly().set_component_instance_transform(
      ComponentInstanceId("component.b"),
      RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{60.0, 0.0, 0.0}}));
  CHECK(angle_alignment_of(rotated_project, "constraint.angle") == Approx(0.0).margin(1.0e-12));
}

TEST_CASE("Assembly numeric system flattens mixed planar and Angle residuals deterministically",
          "[geometry][assembly-angle-solver]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{}}}});
  add_constraint(project, "constraint.a.distance", AssemblyConstraintType::Distance,
                 "feature.base_extrude.top", make_distance(10.0, "constraint.a.distance"));
  add_angle(project, 60.0, "constraint.z.angle");

  auto graph = AssemblyConstraintGraph::build(project.assembly());
  REQUIRE(graph);
  auto constraint_ids =
      detail::collect_constraint_ids(graph.value(), group({"component.a", "component.b"}));
  REQUIRE(constraint_ids);
  REQUIRE(constraint_ids.value().size() == 2U);
  CHECK(constraint_ids.value()[0].value() == "constraint.a.distance");
  CHECK(constraint_ids.value()[1].value() == "constraint.z.angle");

  const auto residuals = detail::evaluate_residuals(project, constraint_ids.value(), 5.0);
  const auto repeated = detail::evaluate_residuals(project, constraint_ids.value(), 5.0);
  REQUIRE(residuals);
  REQUIRE(repeated);
  CHECK(residuals.value() == repeated.value());
  REQUIRE(residuals.value().size() == 5U);

  const std::vector<double> expected{0.0, 0.0, 0.0, 3.0, 0.5};
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    CHECK(residuals.value()[index] == Approx(expected[index]).margin(1.0e-12));
  }
}

TEST_CASE("Rigid-body solver solves the planar Angle family", "[geometry][assembly-angle-solver]") {
  const AssemblyRigidBodySolver solver;
  const AssemblySolveResultApplier applier;

  SECTION("a tilted free component converges to the target angle") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{10.0, 0.0, 0.0}}}});
    add_angle(project, 45.0);
    const RigidTransform source_transform =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();

    const auto first = solver.solve(project, group({"component.a", "component.b"}));
    const auto second = solver.solve(project, group({"component.a", "component.b"}));

    REQUIRE(first);
    REQUIRE(second);
    CHECK(first.value() == second.value());
    REQUIRE(first.value().converged());
    CHECK(first.value().residual_summary.residual_component_count == 1U);
    CHECK(first.value().residual_summary.final_rms <= 1.0e-8);
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == source_transform);

    const auto applied = applier.apply(project, first.value());
    REQUIRE(applied);
    CHECK(applied.value() == 1U);
    CHECK(angle_alignment_of(project, "constraint.angle") == Approx(0.0).margin(kSolveTolerance));
  }

  SECTION("a component already at the target angle stays untouched") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{45.0, 0.0, 0.0}}}});
    add_angle(project, 45.0);
    const RigidTransform source_transform =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();

    const auto solved = solver.solve(project, group({"component.a", "component.b"}));

    REQUIRE(solved);
    REQUIRE(solved.value().converged());
    CHECK(solved.value().iterations == 0U);
    CHECK(proposal_for(solved.value(), "component.b").proposed_transform == source_transform);
  }

  SECTION("grounded components at the wrong angle are fixed-geometry inconsistent") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{}},
          ComponentGroundingState::Grounded}});
    add_angle(project, 45.0);

    const auto solved = solver.solve(project, group({"component.a", "component.b"}));

    REQUIRE(solved);
    CHECK(solved.value().state == AssemblySolveState::FixedGeometryInconsistent);
    CHECK(solved.value().residual_summary.residual_component_count == 1U);
    CHECK(solved.value().proposed_transforms.empty());
  }

  SECTION("non-converged Angle result remains read-only and unapplable") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{10.0, 0.0, 0.0}}}});
    add_angle(project, 45.0);
    const RigidTransform before =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
    AssemblyRigidBodySolverOptions options;
    options.maximum_iterations = 1U;
    options.convergence_rms = 1.0e-16;

    const auto solved = solver.solve(project, group({"component.a", "component.b"}), options);

    REQUIRE(solved);
    CHECK_FALSE(solved.value().converged());
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == before);
    const auto applied = applier.apply(project, solved.value());
    REQUIRE(applied.has_error());
  }
}

TEST_CASE("Assembly solve diagnostics prove Angle rank one with full row rank",
          "[geometry][assembly-angle-solver]") {
  Project project = make_project(
      {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
       {"component.b", RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{45.0, 0.0, 0.0}}}});
  add_angle(project, 45.0);
  const RigidTransform before =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
  const AssemblySolveDiagnosticsAnalyzer analyzer;

  const auto first = analyzer.analyze(project, group({"component.a", "component.b"}));
  const auto second = analyzer.analyze(project, group({"component.a", "component.b"}));

  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == second.value());
  CHECK(first.value().solve_state == AssemblySolveState::Converged);
  CHECK(first.value().dof_classification == AssemblyDofClassification::Underconstrained);
  CHECK(first.value().consistency_classification ==
        AssemblyConstraintConsistencyClassification::LocallyConsistent);
  CHECK(first.value().residual_rank_structure == AssemblyResidualRankStructure::FullRowRank);
  CHECK(first.value().rank_summary.rank_evaluated);
  CHECK(first.value().rank_summary.residual_component_count == 1U);
  CHECK(first.value().rank_summary.variable_count == 6U);
  CHECK(first.value().rank_summary.jacobian_rank == 1U);
  CHECK(first.value().rank_summary.constrained_dof == 1U);
  CHECK(first.value().rank_summary.remaining_dof == 5U);
  CHECK(first.value().rank_summary.residual_row_redundancy == 0U);
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      before);
}

TEST_CASE("Mixed Mate and Angle diagnostics preserve deterministic order and rank",
          "[geometry][assembly-angle-solver]") {
  auto make_mixed_project = [](bool reverse_insertion) {
    // Opposed top normals satisfy Mate; a 180-degree Angle target agrees with
    // that opposed state, so both families are consistent on one pair.
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 16.0}, Vector3{180.0, 0.0, 0.0}}}});
    auto add_mate = [&project] {
      add_constraint(project, "constraint.a.mate", AssemblyConstraintType::Mate,
                     "feature.base_extrude.top");
    };
    auto add_angle_constraint = [&project] {
      add_constraint(project, "constraint.z.angle", AssemblyConstraintType::Angle,
                     "feature.base_extrude.top", std::nullopt,
                     make_angle_value(180.0, "constraint.z.angle"));
    };
    if (reverse_insertion) {
      add_angle_constraint();
      add_mate();
    } else {
      add_mate();
      add_angle_constraint();
    }
    return project;
  };

  const Project forward_project = make_mixed_project(false);
  const Project reverse_project = make_mixed_project(true);
  const AssemblySolveDiagnosticsAnalyzer analyzer;
  const auto forward = analyzer.analyze(forward_project, group({"component.a", "component.b"}));
  const auto reverse = analyzer.analyze(reverse_project, group({"component.a", "component.b"}));

  REQUIRE(forward);
  REQUIRE(reverse);
  CHECK(forward.value() == reverse.value());
  CHECK(forward.value().solve_state == AssemblySolveState::Converged);
  REQUIRE(forward.value().constraint_order.size() == 2U);
  CHECK(forward.value().constraint_order[0].value() == "constraint.a.mate");
  CHECK(forward.value().constraint_order[1].value() == "constraint.z.angle");
  CHECK(forward.value().rank_summary.residual_component_count == 5U);
  CHECK(forward.value().rank_summary.variable_count == 6U);
  CHECK(forward.value().dof_classification == AssemblyDofClassification::Underconstrained);
}
