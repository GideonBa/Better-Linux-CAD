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

PartDocument make_face_part() {
  auto part = PartDocument::create(DocumentId("part.face_plate"), "FacePlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(part.value().add_parameter(
      make_length_parameter("part.thickness", "thickness", 8.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));
  auto sketch =
      Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"),
                                            ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(sketch.value()));
  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base_extrude"), "BaseExtrude", SketchId("sketch.base"),
      ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(part.value().add_feature(feature.value()));
  return part.value();
}

Project make_project(std::initializer_list<ComponentSpec> components) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.face"), "FaceAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.face_plate")));
  for (const auto& spec : components) {
    auto component = ComponentInstance::create(
        ComponentInstanceId(spec.id), spec.id, DocumentId("part.face_plate"),
        ComponentVisibility::Visible, spec.suppression, spec.grounding, spec.transform);
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }

  auto project = Project::create(DocumentId("project.face"), "FaceProject", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_face_part()));
  return project.value();
}

AssemblyConstraintTarget make_target(const char* component, const char* semantic_reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

Quantity make_distance(double millimeters, const char* id) {
  auto distance = Quantity::length_mm(millimeters, id);
  REQUIRE(distance);
  return distance.value();
}

void add_constraint(Project& project,
                    const char* id,
                    AssemblyConstraintType type,
                    const char* component_a,
                    const char* target_a,
                    const char* component_b,
                    const char* target_b,
                    std::optional<Quantity> distance = std::nullopt) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, type, make_target(component_a, target_a),
      make_target(component_b, target_b), AssemblyConstraintState::Active,
      std::move(distance));
  REQUIRE(constraint);
  REQUIRE(project.assembly().add_constraint(constraint.value()));
}

std::vector<ComponentInstanceId> group(std::initializer_list<const char*> ids) {
  std::vector<ComponentInstanceId> result;
  result.reserve(ids.size());
  for (const char* id : ids) {
    result.emplace_back(id);
  }
  return result;
}

} // namespace

TEST_CASE("Assembly solve diagnostics report local remaining DOF for planar constraints",
          "[geometry][assembly-diagnostics]") {
  const AssemblySolveDiagnosticsAnalyzer analyzer;

  SECTION("single Mate leaves three rigid-body DOF") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{5.0, -3.0, 8.0}, Vector3{}}}});
    add_constraint(project, "constraint.mate", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
    const RigidTransform before =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();

    const auto diagnostics = analyzer.analyze(project, group({"component.a", "component.b"}));

    REQUIRE(diagnostics);
    CHECK(diagnostics.value().solve_state == AssemblySolveState::Converged);
    CHECK(diagnostics.value().dof_classification ==
          AssemblyDofClassification::Underconstrained);
    CHECK(diagnostics.value().consistency_classification ==
          AssemblyConstraintConsistencyClassification::LocallyConsistent);
    CHECK(diagnostics.value().residual_rank_structure ==
          AssemblyResidualRankStructure::RedundantResidualComponents);
    REQUIRE(diagnostics.value().variable_components.size() == 1U);
    CHECK(diagnostics.value().variable_components.front().value() == "component.b");
    REQUIRE(diagnostics.value().constraint_order.size() == 1U);
    CHECK(diagnostics.value().constraint_order.front().value() == "constraint.mate");
    CHECK(diagnostics.value().rank_summary.rank_evaluated);
    CHECK(diagnostics.value().rank_summary.residual_component_count == 4U);
    CHECK(diagnostics.value().rank_summary.variable_count == 6U);
    CHECK(diagnostics.value().rank_summary.jacobian_rank == 3U);
    CHECK(diagnostics.value().rank_summary.constrained_dof == 3U);
    CHECK(diagnostics.value().rank_summary.remaining_dof == 3U);
    CHECK(diagnostics.value().rank_summary.residual_row_redundancy == 1U);
    CHECK(project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
          before);
  }

  SECTION("single planar Distance also leaves three rigid-body DOF") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{4.0, 6.0, 12.0}, Vector3{}}}});
    add_constraint(project, "constraint.distance", AssemblyConstraintType::Distance,
                   "component.a", "feature.base_extrude.top", "component.b",
                   "feature.base_extrude.top", make_distance(12.0, "constraint.distance"));

    const auto diagnostics = analyzer.analyze(project, group({"component.a", "component.b"}));

    REQUIRE(diagnostics);
    CHECK(diagnostics.value().rank_summary.jacobian_rank == 3U);
    CHECK(diagnostics.value().rank_summary.constrained_dof == 3U);
    CHECK(diagnostics.value().rank_summary.remaining_dof == 3U);
    CHECK(diagnostics.value().dof_classification ==
          AssemblyDofClassification::Underconstrained);
  }
}

TEST_CASE("Assembly solve diagnostics identify locally full rank and deterministic chains",
          "[geometry][assembly-diagnostics]") {
  const AssemblySolveDiagnosticsAnalyzer analyzer;

  SECTION("three orthogonal Mates locally constrain all six variables") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{120.0, 80.0, 8.0}, Vector3{}}}});
    add_constraint(project, "constraint.x", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.right", "component.b", "feature.base_extrude.left");
    add_constraint(project, "constraint.y", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.front", "component.b", "feature.base_extrude.back");
    add_constraint(project, "constraint.z", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");

    const auto diagnostics = analyzer.analyze(project, group({"component.a", "component.b"}));

    REQUIRE(diagnostics);
    CHECK(diagnostics.value().rank_summary.variable_count == 6U);
    CHECK(diagnostics.value().rank_summary.jacobian_rank == 6U);
    CHECK(diagnostics.value().rank_summary.constrained_dof == 6U);
    CHECK(diagnostics.value().rank_summary.remaining_dof == 0U);
    CHECK(diagnostics.value().dof_classification ==
          AssemblyDofClassification::LocallyFullyConstrained);
    CHECK(diagnostics.value().consistency_classification ==
          AssemblyConstraintConsistencyClassification::LocallyConsistent);
  }

  SECTION("two-Mate chain rank is independent from constraint insertion order") {
    auto make_chain = [](bool reverse_insertion) {
      Project project = make_project(
          {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
           {"component.b", RigidTransform{Vector3{1.0, 2.0, 8.0}, Vector3{}}},
           {"component.c", RigidTransform{Vector3{-4.0, 7.0, 16.0}, Vector3{}}}});
      auto add_ab = [&project] {
        add_constraint(project, "constraint.a.mate_ab", AssemblyConstraintType::Mate,
                       "component.a", "feature.base_extrude.top", "component.b",
                       "feature.base_extrude.bottom");
      };
      auto add_bc = [&project] {
        add_constraint(project, "constraint.z.mate_bc", AssemblyConstraintType::Mate,
                       "component.b", "feature.base_extrude.top", "component.c",
                       "feature.base_extrude.bottom");
      };
      if (reverse_insertion) {
        add_bc();
        add_ab();
      } else {
        add_ab();
        add_bc();
      }
      return project;
    };

    const Project forward_project = make_chain(false);
    const Project reverse_project = make_chain(true);
    const auto forward = analyzer.analyze(
        forward_project, group({"component.a", "component.b", "component.c"}));
    const auto reverse = analyzer.analyze(
        reverse_project, group({"component.a", "component.b", "component.c"}));

    REQUIRE(forward);
    REQUIRE(reverse);
    CHECK(forward.value() == reverse.value());
    CHECK(forward.value().rank_summary.variable_count == 12U);
    CHECK(forward.value().rank_summary.jacobian_rank == 6U);
    CHECK(forward.value().rank_summary.remaining_dof == 6U);
    REQUIRE(forward.value().constraint_order.size() == 2U);
    CHECK(forward.value().constraint_order[0].value() == "constraint.a.mate_ab");
    CHECK(forward.value().constraint_order[1].value() == "constraint.z.mate_bc");
  }
}

TEST_CASE("Assembly solve diagnostics preserve fixed and non-converged solve states",
          "[geometry][assembly-diagnostics]") {
  const AssemblySolveDiagnosticsAnalyzer analyzer;

  SECTION("consistent all-grounded group has no variable DOF") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}},
          ComponentGroundingState::Grounded}});
    add_constraint(project, "constraint.fixed", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");

    const auto diagnostics = analyzer.analyze(project, group({"component.a", "component.b"}));

    REQUIRE(diagnostics);
    CHECK(diagnostics.value().solve_state == AssemblySolveState::Converged);
    CHECK(diagnostics.value().dof_classification ==
          AssemblyDofClassification::NoVariableDof);
    CHECK(diagnostics.value().consistency_classification ==
          AssemblyConstraintConsistencyClassification::LocallyConsistent);
    CHECK(diagnostics.value().rank_summary.rank_evaluated);
    CHECK(diagnostics.value().rank_summary.variable_count == 0U);
    CHECK(diagnostics.value().rank_summary.jacobian_rank == 0U);
    CHECK(diagnostics.value().rank_summary.remaining_dof == 0U);
  }

  SECTION("inconsistent all-grounded group is carried forward explicitly") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", identity_rigid_transform(), ComponentGroundingState::Grounded}});
    add_constraint(project, "constraint.fixed.bad", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.top");

    const auto diagnostics = analyzer.analyze(project, group({"component.a", "component.b"}));

    REQUIRE(diagnostics);
    CHECK(diagnostics.value().solve_state == AssemblySolveState::FixedGeometryInconsistent);
    CHECK(diagnostics.value().dof_classification ==
          AssemblyDofClassification::NotEvaluated);
    CHECK(diagnostics.value().consistency_classification ==
          AssemblyConstraintConsistencyClassification::FixedGeometryInconsistent);
    CHECK(diagnostics.value().residual_rank_structure ==
          AssemblyResidualRankStructure::NotEvaluated);
    CHECK_FALSE(diagnostics.value().rank_summary.rank_evaluated);
  }

  SECTION("non-converged solver result is not mislabeled as a DOF classification") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{}, Vector3{20.0, 0.0, 0.0}}}});
    add_constraint(project, "constraint.distance", AssemblyConstraintType::Distance,
                   "component.a", "feature.base_extrude.top", "component.b",
                   "feature.base_extrude.top", make_distance(1.0, "constraint.distance"));
    AssemblySolveDiagnosticsOptions options;
    options.solver_options.maximum_iterations = 1U;
    options.solver_options.convergence_rms = 1.0e-12;

    const auto diagnostics =
        analyzer.analyze(project, group({"component.a", "component.b"}), options);

    REQUIRE(diagnostics);
    CHECK(diagnostics.value().solve_state == AssemblySolveState::MaximumIterationsReached);
    CHECK(diagnostics.value().dof_classification ==
          AssemblyDofClassification::NotEvaluated);
    CHECK(diagnostics.value().consistency_classification ==
          AssemblyConstraintConsistencyClassification::SolverDidNotConverge);
    CHECK_FALSE(diagnostics.value().rank_summary.rank_evaluated);
    CHECK(diagnostics.value().rank_summary.variable_count == 6U);
  }
}

TEST_CASE("Assembly solve diagnostics expose rank tolerance and residual redundancy boundaries",
          "[geometry][assembly-diagnostics]") {
  Project project = make_project(
      {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
       {"component.b", RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}}}});
  add_constraint(project, "constraint.a", AssemblyConstraintType::Mate, "component.a",
                 "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
  add_constraint(project, "constraint.b", AssemblyConstraintType::Mate, "component.a",
                 "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
  const AssemblySolveDiagnosticsAnalyzer analyzer;

  const auto default_diagnostics = analyzer.analyze(project, group({"component.a", "component.b"}));
  REQUIRE(default_diagnostics);
  CHECK(default_diagnostics.value().rank_summary.residual_component_count == 8U);
  CHECK(default_diagnostics.value().rank_summary.jacobian_rank == 3U);
  CHECK(default_diagnostics.value().rank_summary.residual_row_redundancy == 5U);
  CHECK(default_diagnostics.value().residual_rank_structure ==
        AssemblyResidualRankStructure::RedundantResidualComponents);
  CHECK(default_diagnostics.value().consistency_classification ==
        AssemblyConstraintConsistencyClassification::LocallyConsistent);
  CHECK(default_diagnostics.value().dof_classification ==
        AssemblyDofClassification::Underconstrained);

  AssemblySolveDiagnosticsOptions strict_rank_options;
  strict_rank_options.rank_absolute_tolerance = 10.0;
  strict_rank_options.rank_relative_tolerance = 0.0;
  const auto strict_diagnostics = analyzer.analyze(
      project, group({"component.a", "component.b"}), strict_rank_options);
  REQUIRE(strict_diagnostics);
  CHECK(strict_diagnostics.value().rank_summary.jacobian_rank == 0U);
  CHECK(strict_diagnostics.value().rank_summary.remaining_dof == 6U);
  CHECK(strict_diagnostics.value().rank_summary.pivot_threshold == Approx(10.0));

  AssemblySolveDiagnosticsOptions invalid_options;
  invalid_options.rank_absolute_tolerance = 0.0;
  invalid_options.rank_relative_tolerance = 0.0;
  const auto invalid = analyzer.analyze(
      project, group({"component.a", "component.b"}), invalid_options);
  REQUIRE(invalid.has_error());
  CHECK(invalid.error().message() ==
        "assembly diagnostics rank tolerances must be finite, non-negative, and not both zero");
}

TEST_CASE("Assembly solve diagnostics preserve unsupported solver diagnostics and enum labels",
          "[geometry][assembly-diagnostics]") {
  Project project = make_project(
      {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
       {"component.b", identity_rigid_transform()}});
  add_constraint(project, "constraint.concentric", AssemblyConstraintType::Concentric,
                 "component.a", "bolt.main_axis", "component.b", "bolt.main_axis");

  const AssemblySolveDiagnosticsAnalyzer analyzer;
  const auto diagnostics = analyzer.analyze(project, group({"component.a", "component.b"}));

  REQUIRE(diagnostics.has_error());
  CHECK(diagnostics.error().message() ==
        "concentric equation construction requires semantic axis target support");
  CHECK(to_string(AssemblyDofClassification::LocallyFullyConstrained) ==
        "locally_fully_constrained");
  CHECK(to_string(AssemblyConstraintConsistencyClassification::FixedGeometryInconsistent) ==
        "fixed_geometry_inconsistent");
  CHECK(to_string(AssemblyResidualRankStructure::RedundantResidualComponents) ==
        "redundant_residual_components");
}
