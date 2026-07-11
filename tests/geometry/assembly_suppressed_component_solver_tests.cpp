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
  auto assembly = AssemblyDocument::create(DocumentId("assembly.suppressed"), "Suppressed");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.plate")));
  for (const auto& spec : components) {
    auto component = ComponentInstance::create(
        ComponentInstanceId(spec.id), spec.id, DocumentId("part.plate"),
        ComponentVisibility::Visible, spec.suppression, spec.grounding, spec.transform);
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }

  auto project =
      Project::create(DocumentId("project.suppressed"), "SuppressedProject", assembly.value());
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

void add_mate(Project& project, const char* id, const char* component_a, const char* face_a,
              const char* component_b, const char* face_b) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Mate, make_target(component_a, face_a),
      make_target(component_b, face_b));
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

void set_suppression(Project& project, const char* id, ComponentSuppressionState state) {
  REQUIRE(
      project.assembly().set_component_instance_suppression_state(ComponentInstanceId(id), state));
}

} // namespace

TEST_CASE("A suppressed middle component splits the numeric system deterministically",
          "[geometry][assembly-suppressed]") {
  // Chain a - b - c: b is suppressed, so both constraints vanish and a/c stay.
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", RigidTransform{Vector3{0.0, 0.0, 30.0}, Vector3{}},
                     ComponentGroundingState::Free, ComponentSuppressionState::Suppressed},
                    {"component.c", RigidTransform{Vector3{0.0, 0.0, 60.0}, Vector3{}}}});
  add_mate(project, "constraint.ab", "component.a", "feature.base_extrude.top", "component.b",
           "feature.base_extrude.bottom");
  add_mate(project, "constraint.bc", "component.b", "feature.base_extrude.top", "component.c",
           "feature.base_extrude.bottom");

  const AssemblyRigidBodySolver solver;
  const auto first = solver.solve(project, group({"component.a", "component.b", "component.c"}));
  const auto second = solver.solve(project, group({"component.a", "component.b", "component.c"}));

  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == second.value());
  CHECK(first.value().state == AssemblySolveState::Converged);
  CHECK(first.value().iterations == 0U);
  CHECK(first.value().residual_summary.residual_component_count == 0U);
  CHECK(first.value().component_snapshots.size() == 3U);
  // component.c stays free and unconstrained; its proposal keeps the source pose.
  REQUIRE(first.value().proposed_transforms.size() == 1U);
  CHECK(first.value().proposed_transforms.front().component_instance.value() == "component.c");
  CHECK(first.value().proposed_transforms.front().proposed_transform ==
        first.value().proposed_transforms.front().source_transform);
}

TEST_CASE("A suppressed grounded component keeps the remaining subgroup solvable",
          "[geometry][assembly-suppressed]") {
  // a (grounded, suppressed) and b (grounded) both constrain c; only b-c stays.
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded,
                     ComponentSuppressionState::Suppressed},
                    {"component.b", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.c", RigidTransform{Vector3{3.0, -4.0, 25.0}, Vector3{}}}});
  add_mate(project, "constraint.ac", "component.a", "feature.base_extrude.top", "component.c",
           "feature.base_extrude.bottom");
  add_mate(project, "constraint.bc", "component.b", "feature.base_extrude.top", "component.c",
           "feature.base_extrude.bottom");

  const AssemblyRigidBodySolver solver;
  const AssemblySolveResultApplier applier;
  const auto solved = solver.solve(project, group({"component.a", "component.b", "component.c"}));

  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  // Only the b-c Mate remains: four residual components.
  CHECK(solved.value().residual_summary.residual_component_count == 4U);
  CHECK(solved.value().fixed_components.size() == 1U);
  CHECK(solved.value().fixed_components.front().value() == "component.b");

  const auto applied = applier.apply(project, solved.value());
  REQUIRE(applied);
  const RigidTransform& c_transform =
      project.assembly().find_component_instance(ComponentInstanceId("component.c"))->transform();
  // b's top face sits at z = 8; c's bottom mates onto it.
  CHECK(c_transform.translation_mm.z == Approx(8.0).margin(kSolveTolerance));
}

TEST_CASE("A subgroup without a grounded active component is still rejected",
          "[geometry][assembly-suppressed]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded,
                     ComponentSuppressionState::Suppressed},
                    {"component.b", identity_rigid_transform()},
                    {"component.c", RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{}}}});
  add_mate(project, "constraint.ab", "component.a", "feature.base_extrude.top", "component.b",
           "feature.base_extrude.bottom");
  add_mate(project, "constraint.bc", "component.b", "feature.base_extrude.top", "component.c",
           "feature.base_extrude.bottom");

  const AssemblyRigidBodySolver solver;
  const auto solved = solver.solve(project, group({"component.a", "component.b", "component.c"}));

  REQUIRE(solved.has_error());
  CHECK(solved.error().message() ==
        "solver connected group requires at least one grounded component");
}

TEST_CASE("Unsuppressing a component after solving makes the result stale",
          "[geometry][assembly-suppressed]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.c", RigidTransform{Vector3{3.0, -4.0, 25.0}, Vector3{}}}});
  add_mate(project, "constraint.ac", "component.a", "feature.base_extrude.top", "component.c",
           "feature.base_extrude.bottom");
  add_mate(project, "constraint.bc", "component.b", "feature.base_extrude.top", "component.c",
           "feature.base_extrude.bottom");
  set_suppression(project, "component.a", ComponentSuppressionState::Suppressed);

  const AssemblyRigidBodySolver solver;
  const auto solved = solver.solve(project, group({"component.a", "component.b", "component.c"}));
  REQUIRE(solved);
  REQUIRE(solved.value().converged());

  set_suppression(project, "component.a", ComponentSuppressionState::Active);

  const AssemblySolveResultApplier applier;
  const auto applied = applier.apply(project, solved.value());
  REQUIRE(applied.has_error());
  CHECK(applied.error().message() ==
        "assembly solve result is stale because component solve input changed");
}

TEST_CASE("Suppression diagnostics report rank only over active components",
          "[geometry][assembly-suppressed]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded,
                     ComponentSuppressionState::Suppressed},
                    {"component.b", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.c", RigidTransform{Vector3{3.0, -4.0, 25.0}, Vector3{}}}});
  add_mate(project, "constraint.ac", "component.a", "feature.base_extrude.top", "component.c",
           "feature.base_extrude.bottom");
  add_mate(project, "constraint.bc", "component.b", "feature.base_extrude.top", "component.c",
           "feature.base_extrude.bottom");

  const AssemblySolveDiagnosticsAnalyzer analyzer;
  const auto first =
      analyzer.analyze(project, group({"component.a", "component.b", "component.c"}));
  const auto second =
      analyzer.analyze(project, group({"component.a", "component.b", "component.c"}));

  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == second.value());
  CHECK(first.value().solve_state == AssemblySolveState::Converged);
  REQUIRE(first.value().constraint_order.size() == 1U);
  CHECK(first.value().constraint_order.front().value() == "constraint.bc");
  // One free active component and one remaining Mate: rank 3 of 6 variables.
  CHECK(first.value().rank_summary.residual_component_count == 4U);
  CHECK(first.value().rank_summary.variable_count == 6U);
  CHECK(first.value().rank_summary.jacobian_rank == 3U);
  CHECK(first.value().rank_summary.remaining_dof == 3U);
  CHECK(first.value().dof_classification == AssemblyDofClassification::Underconstrained);
}
