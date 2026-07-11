#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include "blcad/geometry/assembly_constraint_equation_builder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
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
  REQUIRE(part.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));
  auto sketch = Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(sketch.value()));
  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
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

} // namespace

TEST_CASE("Rigid-body solver solves and explicitly applies one movable planar Mate",
          "[geometry][assembly-solver]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", RigidTransform{Vector3{5.0, -3.0, 20.0}, Vector3{}}}});
  add_constraint(project, "constraint.mate", AssemblyConstraintType::Mate, "component.a",
                 "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");

  const RigidTransform source_transform =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
  const AssemblyRigidBodySolver solver;
  const auto first = solver.solve(project, group({"component.a", "component.b"}));
  const auto second = solver.solve(project, group({"component.a", "component.b"}));

  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == second.value());
  REQUIRE(first.value().converged());
  CHECK(first.value().iterations > 0U);
  REQUIRE(first.value().fixed_components.size() == 1U);
  CHECK(first.value().fixed_components.front().value() == "component.a");
  REQUIRE(first.value().component_snapshots.size() == 2U);
  REQUIRE(first.value().proposed_transforms.size() == 1U);
  const auto& proposal = proposal_for(first.value(), "component.b");
  CHECK(proposal.source_transform == source_transform);
  CHECK(proposal.proposed_transform.translation_mm.x == Approx(5.0).margin(kSolveTolerance));
  CHECK(proposal.proposed_transform.translation_mm.y == Approx(-3.0).margin(kSolveTolerance));
  CHECK(proposal.proposed_transform.translation_mm.z == Approx(8.0).margin(kSolveTolerance));
  CHECK(first.value().residual_summary.initial_rms > first.value().residual_summary.final_rms);
  CHECK(first.value().residual_summary.final_rms <= 1.0e-8);
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      source_transform);

  const AssemblySolveResultApplier applier;
  const auto applied = applier.apply(project, first.value());
  REQUIRE(applied);
  CHECK(applied.value() == 1U);

  const AssemblyConstraintEquationBuilder builder;
  const auto equation = builder.build(project, project.assembly().constraints().front());
  REQUIRE(equation);
  const auto& residual = std::get<PlanarMateResidualDescriptor>(equation.value().residual);
  CHECK(residual.normal_opposition.x == Approx(0.0).margin(kSolveTolerance));
  CHECK(residual.normal_opposition.y == Approx(0.0).margin(kSolveTolerance));
  CHECK(residual.normal_opposition.z == Approx(0.0).margin(kSolveTolerance));
  CHECK(residual.signed_separation_mm == Approx(0.0).margin(kSolveTolerance));
}

TEST_CASE("Rigid-body solver corrects planar Distance translation and orientation",
          "[geometry][assembly-solver]") {
  const AssemblyRigidBodySolver solver;

  SECTION("signed separation") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{4.0, 6.0, -2.0}, Vector3{}}}});
    add_constraint(project, "constraint.distance", AssemblyConstraintType::Distance, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.top",
                   make_distance(12.0, "constraint.distance"));

    const auto solved = solver.solve(project, group({"component.a", "component.b"}));
    REQUIRE(solved);
    REQUIRE(solved.value().converged());
    const auto& proposal = proposal_for(solved.value(), "component.b");
    CHECK(proposal.proposed_transform.translation_mm.x == Approx(4.0).margin(kSolveTolerance));
    CHECK(proposal.proposed_transform.translation_mm.y == Approx(6.0).margin(kSolveTolerance));
    CHECK(proposal.proposed_transform.translation_mm.z == Approx(12.0).margin(kSolveTolerance));
  }

  SECTION("nonparallel plane normals") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{}, Vector3{20.0, 0.0, 0.0}}}});
    add_constraint(project, "constraint.distance.rotate", AssemblyConstraintType::Distance,
                   "component.a", "feature.base_extrude.top", "component.b",
                   "feature.base_extrude.top", make_distance(1.0, "constraint.distance.rotate"));

    const auto solved = solver.solve(project, group({"component.a", "component.b"}));
    REQUIRE(solved);
    REQUIRE(solved.value().converged());
    const auto& proposal = proposal_for(solved.value(), "component.b");
    CHECK(proposal.proposed_transform.rotation_deg.x == Approx(0.0).margin(kSolveTolerance));
    CHECK(proposal.proposed_transform.rotation_deg.y == Approx(0.0).margin(kSolveTolerance));
    CHECK(proposal.proposed_transform.translation_mm.z == Approx(1.0).margin(kSolveTolerance));
    CHECK(solved.value().residual_summary.final_rms <= 1.0e-8);
  }
}

TEST_CASE("Rigid-body solver uses deterministic constraint and variable ordering for a chain",
          "[geometry][assembly-solver]") {
  auto make_chain = [](bool reverse_insertion) {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{1.0, 2.0, 20.0}, Vector3{}}},
         {"component.c", RigidTransform{Vector3{-4.0, 7.0, 40.0}, Vector3{}}}});
    auto add_ab = [&project] {
      add_constraint(project, "constraint.a.mate_ab", AssemblyConstraintType::Mate, "component.a",
                     "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
    };
    auto add_bc = [&project] {
      add_constraint(project, "constraint.z.mate_bc", AssemblyConstraintType::Mate, "component.b",
                     "feature.base_extrude.top", "component.c", "feature.base_extrude.bottom");
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
  const AssemblyRigidBodySolver solver;
  const auto forward =
      solver.solve(forward_project, group({"component.a", "component.b", "component.c"}));
  const auto reverse =
      solver.solve(reverse_project, group({"component.a", "component.b", "component.c"}));

  REQUIRE(forward);
  REQUIRE(reverse);
  REQUIRE(forward.value().converged());
  REQUIRE(reverse.value().converged());
  CHECK(forward.value() == reverse.value());
  CHECK(proposal_for(forward.value(), "component.b").proposed_transform.translation_mm.z ==
        Approx(8.0).margin(kSolveTolerance));
  CHECK(proposal_for(forward.value(), "component.c").proposed_transform.translation_mm.z ==
        Approx(16.0).margin(kSolveTolerance));
}

TEST_CASE("Rigid-body solver defines grounded, suppressed, and connected-group participation",
          "[geometry][assembly-solver]") {
  const AssemblyRigidBodySolver solver;

  SECTION("multiple grounded components are all fixed") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}},
          ComponentGroundingState::Grounded}});
    add_constraint(project, "constraint.fixed", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
    const auto solved = solver.solve(project, group({"component.a", "component.b"}));
    REQUIRE(solved);
    CHECK(solved.value().state == AssemblySolveState::Converged);
    CHECK(solved.value().iterations == 0U);
    CHECK(solved.value().fixed_components.size() == 2U);
    CHECK(solved.value().proposed_transforms.empty());
  }

  SECTION("inconsistent all-grounded geometry is diagnosed") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", identity_rigid_transform(), ComponentGroundingState::Grounded}});
    add_constraint(project, "constraint.fixed.bad", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.top");
    const auto solved = solver.solve(project, group({"component.a", "component.b"}));
    REQUIRE(solved);
    CHECK(solved.value().state == AssemblySolveState::FixedGeometryInconsistent);
    CHECK(solved.value().proposed_transforms.empty());
  }

  SECTION("zero grounded components are rejected") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform()}, {"component.b", identity_rigid_transform()}});
    add_constraint(project, "constraint.mate", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
    const auto solved = solver.solve(project, group({"component.a", "component.b"}));
    REQUIRE(solved.has_error());
    CHECK(solved.error().message() ==
          "solver connected group requires at least one grounded component");
  }

  SECTION("a suppressed component removes its constraints instead of failing the solve") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", identity_rigid_transform(), ComponentGroundingState::Free,
          ComponentSuppressionState::Suppressed}});
    add_constraint(project, "constraint.mate", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
    const auto solved = solver.solve(project, group({"component.a", "component.b"}));
    REQUIRE(solved);
    CHECK(solved.value().state == AssemblySolveState::Converged);
    CHECK(solved.value().iterations == 0U);
    CHECK(solved.value().residual_summary.residual_component_count == 0U);
    CHECK(solved.value().proposed_transforms.empty());
    CHECK(solved.value().component_snapshots.size() == 2U);
  }

  SECTION("input must exactly match deterministic graph group order") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", identity_rigid_transform()}});
    add_constraint(project, "constraint.mate", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
    const auto solved = solver.solve(project, group({"component.b", "component.a"}));
    REQUIRE(solved.has_error());
    CHECK(solved.error().message() ==
          "solver input must exactly match one deterministic connected component");
  }
}

TEST_CASE("Rigid-body solver exposes validation and semantic target failures",
          "[geometry][assembly-solver]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", identity_rigid_transform()}});
  const AssemblyRigidBodySolver solver;

  SECTION("invalid numeric options") {
    add_constraint(project, "constraint.mate", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
    AssemblyRigidBodySolverOptions options;
    options.length_residual_scale_mm = 0.0;
    const auto solved = solver.solve(project, group({"component.a", "component.b"}), options);
    REQUIRE(solved.has_error());
    CHECK(solved.error().message() == "solver length residual scale must be finite and positive");
  }

  SECTION("unsupported Concentric semantic axis family is rejected") {
    add_constraint(project, "constraint.concentric", AssemblyConstraintType::Concentric,
                   "component.a", "bolt.main_axis", "component.b", "bolt.main_axis");
    const auto solved = solver.solve(project, group({"component.a", "component.b"}));
    REQUIRE(solved.has_error());
    CHECK(solved.error().message() == "unsupported assembly semantic axis reference family");
  }
}

TEST_CASE("Non-converged and stale solve results cannot mutate project placement",
          "[geometry][assembly-solver]") {
  const AssemblyRigidBodySolver solver;
  const AssemblySolveResultApplier applier;

  SECTION("maximum iteration result") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{}, Vector3{20.0, 0.0, 0.0}}}});
    add_constraint(project, "constraint.distance", AssemblyConstraintType::Distance, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.top",
                   make_distance(1.0, "constraint.distance"));
    const RigidTransform before =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
    AssemblyRigidBodySolverOptions options;
    options.maximum_iterations = 1U;
    options.convergence_rms = 1.0e-12;

    const auto solved = solver.solve(project, group({"component.a", "component.b"}), options);
    REQUIRE(solved);
    CHECK(solved.value().state == AssemblySolveState::MaximumIterationsReached);
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == before);
    const auto applied = applier.apply(project, solved.value());
    REQUIRE(applied.has_error());
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == before);
  }

  SECTION("changed grounded anchor makes a converged result stale") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 20.0}, Vector3{}}}});
    add_constraint(project, "constraint.mate", AssemblyConstraintType::Mate, "component.a",
                   "feature.base_extrude.top", "component.b", "feature.base_extrude.bottom");
    const auto solved = solver.solve(project, group({"component.a", "component.b"}));
    REQUIRE(solved);
    REQUIRE(solved.value().converged());
    const RigidTransform component_b_before =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
    REQUIRE(project.assembly().set_component_instance_transform(
        ComponentInstanceId("component.a"), RigidTransform{Vector3{1.0, 0.0, 0.0}, Vector3{}}));

    const auto applied = applier.apply(project, solved.value());
    REQUIRE(applied.has_error());
    CHECK(applied.error().message() ==
          "assembly solve result is stale because component solve input changed");
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == component_b_before);
  }
}
