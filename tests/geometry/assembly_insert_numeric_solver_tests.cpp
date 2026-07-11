#include "assembly_constraint_numeric_system.hpp"

#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_solve_diagnostics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <optional>
#include <utility>
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
  ComponentSuppressionState suppression = ComponentSuppressionState::Active;
};

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_hole_part() {
  auto part = PartDocument::create(DocumentId("part.hole_plate"), "HolePlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(part.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

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

  auto hole_sketch =
      Sketch::create(SketchId("sketch.hole"), "HoleSketch", DatumPlaneId("datum.xy"));
  REQUIRE(hole_sketch);
  auto circle = CircleProfile::create(ProfileId("profile.hole"), ParameterId("part.hole_diameter"),
                                      Point2{0.0, 0.0});
  REQUIRE(circle);
  REQUIRE(hole_sketch.value().add_profile(circle.value()));
  REQUIRE(part.value().add_sketch(hole_sketch.value()));

  auto hole_feature = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"), FeatureId("feature.base_extrude"),
      SubtractiveExtrudeDepth::ThroughAll, ExtrudeDirection::SketchNormal);
  REQUIRE(hole_feature);
  REQUIRE(part.value().add_feature(hole_feature.value()));

  return part.value();
}

Project make_project(std::initializer_list<ComponentSpec> components) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.inserts"), "InsertAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.hole_plate")));
  for (const auto& spec : components) {
    auto component = ComponentInstance::create(
        ComponentInstanceId(spec.id), spec.id, DocumentId("part.hole_plate"),
        ComponentVisibility::Visible, spec.suppression, spec.grounding, spec.transform);
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }

  auto project = Project::create(DocumentId("project.inserts"), "InsertProject", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_hole_part()));
  return project.value();
}

AssemblyConstraintTarget make_target(const char* component, const char* semantic_reference) {
  auto target =
      AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

Quantity make_distance(double millimeters, const char* id) {
  auto distance = Quantity::length_mm(millimeters, id);
  REQUIRE(distance);
  return distance.value();
}

void add_constraint(Project& project, const char* id, AssemblyConstraintType type,
                    const char* component_a, const char* target_a, const char* component_b,
                    const char* target_b, std::optional<Quantity> distance = std::nullopt) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, type, make_target(component_a, target_a),
      make_target(component_b, target_b), AssemblyConstraintState::Active, std::move(distance));
  REQUIRE(constraint);
  REQUIRE(project.assembly().add_constraint(constraint.value()));
}

void add_insert(Project& project, const char* id) {
  add_constraint(project, id, AssemblyConstraintType::Insert, "component.a", "feature.hole.seat",
                 "component.b", "feature.hole.seat");
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

void check_vector_near_zero(const Vector3& value, double tolerance = kSolveTolerance) {
  CHECK(value.x == Approx(0.0).margin(tolerance));
  CHECK(value.y == Approx(0.0).margin(tolerance));
  CHECK(value.z == Approx(0.0).margin(tolerance));
}

void check_insert_residual_near_zero(const Project& project, const char* constraint_id) {
  const AssemblyConstraint* constraint =
      project.assembly().find_constraint(AssemblyConstraintId(constraint_id));
  REQUIRE(constraint != nullptr);
  const AssemblyInsertConstraintEquationBuilder builder;
  const auto equation = builder.build(project, *constraint);
  REQUIRE(equation);
  check_vector_near_zero(equation.value().residual.direction_parallelism);
  check_vector_near_zero(equation.value().residual.axis_offset_mm);
  CHECK(equation.value().residual.signed_seating_separation_mm ==
        Approx(0.0).margin(kSolveTolerance));
}

} // namespace

TEST_CASE("Assembly numeric system flattens mixed planar and Insert residuals deterministically",
          "[geometry][assembly-insert-solver]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", RigidTransform{Vector3{3.0, 0.0, 25.0}, Vector3{}}}});
  add_constraint(project, "constraint.a.distance", AssemblyConstraintType::Distance, "component.a",
                 "feature.base_extrude.top", "component.b", "feature.base_extrude.top",
                 make_distance(10.0, "constraint.a.distance"));
  add_constraint(project, "constraint.z.insert", AssemblyConstraintType::Insert, "component.a",
                 "feature.hole.seat", "component.b", "feature.hole.seat");

  auto graph = AssemblyConstraintGraph::build(project.assembly());
  REQUIRE(graph);
  auto constraint_ids =
      detail::collect_constraint_ids(graph.value(), group({"component.a", "component.b"}));
  REQUIRE(constraint_ids);
  REQUIRE(constraint_ids.value().size() == 2U);
  CHECK(constraint_ids.value()[0].value() == "constraint.a.distance");
  CHECK(constraint_ids.value()[1].value() == "constraint.z.insert");

  const auto residuals = detail::evaluate_residuals(project, constraint_ids.value(), 5.0);
  const auto repeated = detail::evaluate_residuals(project, constraint_ids.value(), 5.0);
  REQUIRE(residuals);
  REQUIRE(repeated);
  CHECK(residuals.value() == repeated.value());
  REQUIRE(residuals.value().size() == 11U);

  const std::vector<double> expected{0.0, 0.0, 0.0, 3.0, 0.0, 0.0, 0.0, 0.0, -0.6, 0.0, 5.0};
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    CHECK(residuals.value()[index] == Approx(expected[index]).margin(1.0e-12));
  }

  const std::vector<ComponentInstanceId> variable_components{ComponentInstanceId("component.b")};
  const detail::NumericVector variables = detail::read_variables(project, variable_components);
  const detail::AssemblyNumericSystemOptions options{5.0, 1.0e-4, 1.0e-4};
  const auto jacobian = detail::build_central_difference_jacobian(
      project, variable_components, constraint_ids.value(), variables, residuals.value(), options);

  REQUIRE(jacobian);
  REQUIRE(jacobian.value().size() == 11U);
  for (const auto& row : jacobian.value()) {
    CHECK(row.size() == 6U);
  }
}

TEST_CASE("Rigid-body solver solves Insert lateral offset, tilt, and axial seating",
          "[geometry][assembly-insert-solver]") {
  const AssemblyRigidBodySolver solver;
  const AssemblySolveResultApplier applier;

  SECTION("lateral offset and axial separation are corrected while axis rotation is preserved") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{3.0, -4.0, 7.0}, Vector3{0.0, 0.0, 33.0}}}});
    add_insert(project, "constraint.insert");
    const RigidTransform source_transform =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();

    const auto first = solver.solve(project, group({"component.a", "component.b"}));
    const auto second = solver.solve(project, group({"component.a", "component.b"}));

    REQUIRE(first);
    REQUIRE(second);
    CHECK(first.value() == second.value());
    REQUIRE(first.value().converged());
    CHECK(first.value().residual_summary.residual_component_count == 7U);
    CHECK(first.value().residual_summary.final_rms <= 1.0e-8);
    const auto& proposal = proposal_for(first.value(), "component.b");
    CHECK(proposal.proposed_transform.translation_mm.x == Approx(0.0).margin(kSolveTolerance));
    CHECK(proposal.proposed_transform.translation_mm.y == Approx(0.0).margin(kSolveTolerance));
    CHECK(proposal.proposed_transform.translation_mm.z == Approx(0.0).margin(kSolveTolerance));
    CHECK(proposal.proposed_transform.rotation_deg.x == Approx(0.0).margin(kSolveTolerance));
    CHECK(proposal.proposed_transform.rotation_deg.y == Approx(0.0).margin(kSolveTolerance));
    CHECK(proposal.proposed_transform.rotation_deg.z == Approx(33.0).margin(kSolveTolerance));
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == source_transform);

    const auto applied = applier.apply(project, first.value());
    REQUIRE(applied);
    CHECK(applied.value() == 1U);
    check_insert_residual_near_zero(project, "constraint.insert");
  }

  SECTION("axis tilt and seating are corrected through the shared finite-difference solver path") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 5.0}, Vector3{20.0, -15.0, 0.0}}}});
    add_insert(project, "constraint.insert");

    const auto solved = solver.solve(project, group({"component.a", "component.b"}));

    REQUIRE(solved);
    REQUIRE(solved.value().converged());
    CHECK(solved.value().iterations > 0U);
    CHECK(solved.value().residual_summary.initial_rms > solved.value().residual_summary.final_rms);

    Project evaluation_project = project;
    REQUIRE(applier.apply(evaluation_project, solved.value()));
    check_insert_residual_near_zero(evaluation_project, "constraint.insert");
  }

  SECTION("equal and opposed aligned axis directions are already valid Insert states") {
    for (const Vector3 rotation : {Vector3{0.0, 0.0, 47.0}, Vector3{180.0, 0.0, 47.0}}) {
      Project project = make_project(
          {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
           {"component.b", RigidTransform{Vector3{}, rotation}}});
      add_insert(project, "constraint.insert");
      const RigidTransform source_transform =
          project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform();

      const auto solved = solver.solve(project, group({"component.a", "component.b"}));

      REQUIRE(solved);
      REQUIRE(solved.value().converged());
      CHECK(solved.value().iterations == 0U);
      CHECK(proposal_for(solved.value(), "component.b").proposed_transform == source_transform);
    }
  }
}

TEST_CASE("Insert solver preserves fixed inconsistency and non-convergence boundaries",
          "[geometry][assembly-insert-solver]") {
  const AssemblyRigidBodySolver solver;
  const AssemblySolveResultApplier applier;

  SECTION("offset grounded seats are fixed-geometry inconsistent") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{3.0, 0.0, 25.0}, Vector3{}},
          ComponentGroundingState::Grounded}});
    add_insert(project, "constraint.insert");

    const auto solved = solver.solve(project, group({"component.a", "component.b"}));

    REQUIRE(solved);
    CHECK(solved.value().state == AssemblySolveState::FixedGeometryInconsistent);
    CHECK(solved.value().residual_summary.residual_component_count == 7U);
    CHECK(solved.value().proposed_transforms.empty());
  }

  SECTION("non-converged Insert result remains read-only and unapplable") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{10.0, -7.0, 25.0}, Vector3{45.0, -30.0, 10.0}}}});
    add_insert(project, "constraint.insert");
    const RigidTransform before =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
    AssemblyRigidBodySolverOptions options;
    options.maximum_iterations = 1U;
    options.convergence_rms = 1.0e-14;

    const auto solved = solver.solve(project, group({"component.a", "component.b"}), options);

    REQUIRE(solved);
    CHECK_FALSE(solved.value().converged());
    CHECK((solved.value().state == AssemblySolveState::MaximumIterationsReached ||
           solved.value().state == AssemblySolveState::NumericalFailure));
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == before);
    const auto applied = applier.apply(project, solved.value());
    REQUIRE(applied.has_error());
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == before);
  }
}

TEST_CASE("Assembly solve diagnostics prove Insert rank five and one remaining DOF",
          "[geometry][assembly-insert-solver]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", RigidTransform{Vector3{}, Vector3{0.0, 0.0, 37.0}}}});
  add_insert(project, "constraint.insert");
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
  CHECK(first.value().residual_rank_structure ==
        AssemblyResidualRankStructure::RedundantResidualComponents);
  CHECK(first.value().rank_summary.rank_evaluated);
  CHECK(first.value().rank_summary.residual_component_count == 7U);
  CHECK(first.value().rank_summary.variable_count == 6U);
  CHECK(first.value().rank_summary.jacobian_rank == 5U);
  CHECK(first.value().rank_summary.constrained_dof == 5U);
  CHECK(first.value().rank_summary.remaining_dof == 1U);
  CHECK(first.value().rank_summary.residual_row_redundancy == 2U);
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      before);
}

TEST_CASE("Mixed Concentric and Insert diagnostics preserve deterministic order and rank",
          "[geometry][assembly-insert-solver]") {
  auto make_mixed_project = [](bool reverse_insertion) {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{}, Vector3{0.0, 0.0, 30.0}}}});
    auto add_concentric = [&project] {
      add_constraint(project, "constraint.a.concentric", AssemblyConstraintType::Concentric,
                     "component.a", "feature.hole.axis", "component.b", "feature.hole.axis");
    };
    auto add_insert_constraint = [&project] {
      add_constraint(project, "constraint.z.insert", AssemblyConstraintType::Insert, "component.a",
                     "feature.hole.seat", "component.b", "feature.hole.seat");
    };
    if (reverse_insertion) {
      add_insert_constraint();
      add_concentric();
    } else {
      add_concentric();
      add_insert_constraint();
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
  CHECK(forward.value().constraint_order[0].value() == "constraint.a.concentric");
  CHECK(forward.value().constraint_order[1].value() == "constraint.z.insert");
  CHECK(forward.value().rank_summary.residual_component_count == 13U);
  CHECK(forward.value().rank_summary.variable_count == 6U);
  CHECK(forward.value().rank_summary.jacobian_rank == 5U);
  CHECK(forward.value().rank_summary.constrained_dof == 5U);
  CHECK(forward.value().rank_summary.remaining_dof == 1U);
  CHECK(forward.value().rank_summary.residual_row_redundancy == 8U);
  CHECK(forward.value().dof_classification == AssemblyDofClassification::Underconstrained);
}
