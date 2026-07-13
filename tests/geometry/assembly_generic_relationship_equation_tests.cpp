#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/geometry/assembly_generic_relationship_equation_builder.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/assembly_solve_diagnostics.hpp"
#include "blcad/geometry/assembly_target_compatibility.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
namespace ts = blcad::geometry::test_support;
using Catch::Approx;

namespace {

constexpr double kTolerance = 1.0e-6;

AssemblyResolvedGeometricTarget synthetic_target(AssemblyGeometricTargetSourceKind kind,
                                                 AssemblyGeometricTargetDescriptor descriptor,
                                                 const char* semantic_reference) {
  AssemblyGeometricTargetSourceMetadata metadata;
  metadata.referenced_part_document = DocumentId("part.synthetic");
  if (kind == AssemblyGeometricTargetSourceKind::GeneratedPlanarFace ||
      kind == AssemblyGeometricTargetSourceKind::GeneratedLinearEdge ||
      kind == AssemblyGeometricTargetSourceKind::GeneratedVertex) {
    metadata.source_feature = FeatureId("feature.synthetic");
  }
  if (kind == AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace) {
    metadata.source_feature = FeatureId("feature.synthetic");
    metadata.source_profile = ProfileId("profile.synthetic");
  }
  return AssemblyResolvedGeometricTarget{
      AssemblyHierarchyGeometricTargetEndpointIdentity{
          {}, ComponentInstanceId("component.synthetic"), semantic_reference},
      kind,
      std::move(metadata),
      std::move(descriptor),
      assembly_geometric_target_capabilities(kind),
      AssemblyGeometricTargetCoordinateSpace::RootAssembly,
      {identity_rigid_transform()}};
}

AssemblyResolvedGeometricTarget point_target(Point3 point, const char* reference = "point") {
  return synthetic_target(AssemblyGeometricTargetSourceKind::ConstructionPoint,
                          AssemblyPointTargetDescriptor{point}, reference);
}

AssemblyResolvedGeometricTarget line_target(Point3 origin, Vector3 direction,
                                            const char* reference = "line") {
  return synthetic_target(AssemblyGeometricTargetSourceKind::ConstructionLine,
                          AssemblyLineTargetDescriptor{origin, direction}, reference);
}

AssemblyResolvedGeometricTarget axis_target(Point3 origin, Vector3 direction,
                                            const char* reference = "axis") {
  return synthetic_target(AssemblyGeometricTargetSourceKind::DatumAxis,
                          AssemblyAxisTargetDescriptor{origin, direction}, reference);
}

AssemblyResolvedGeometricTarget cylinder_axis_target(Point3 origin, Vector3 direction,
                                                     const char* reference = "cylinder.axis") {
  return synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace,
                          AssemblyCylindricalSurfaceTargetDescriptor{origin, direction, 2.0},
                          reference);
}

AssemblyResolvedGeometricTarget plane_target(Point3 origin, Vector3 normal,
                                             const char* reference = "plane") {
  const Vector3 x_axis = std::abs(normal.x) > 0.5 ? Vector3{0.0, 1.0, 0.0} : Vector3{1.0, 0.0, 0.0};
  const Vector3 y_axis = std::abs(normal.x) > 0.5 ? Vector3{0.0, 0.0, 1.0} : Vector3{0.0, 1.0, 0.0};
  return synthetic_target(AssemblyGeometricTargetSourceKind::DatumPlane,
                          AssemblyPlanarTargetDescriptor{origin, x_axis, y_axis, normal},
                          reference);
}

Quantity angle(double degrees, const char* id) {
  auto value = Quantity::angle_deg(degrees, id);
  REQUIRE(value);
  return value.value();
}

template <typename Id> std::string reference_spelling(Id id) {
  auto spelling = make_assembly_reference_target_spelling(AssemblyReferenceTargetIdentity{id});
  REQUIRE(spelling);
  return spelling.value();
}

AssemblyConstraintTarget local_target(const char* component_id, const std::string& reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component_id), reference);
  REQUIRE(target);
  return target.value();
}

AssemblyHierarchyConstraintEndpoint hierarchy_target(std::initializer_list<const char*> path,
                                                     const char* component_id,
                                                     const std::string& reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const char* id : path) {
    occurrence_path.emplace_back(id);
  }
  auto target = AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path), ComponentInstanceId(component_id), reference);
  REQUIRE(target);
  return target.value();
}

Project local_coincident_project() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.generic.local"), "GenericLocal");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(assembly.value().add_component_instance(
      ts::component("component.a", ComponentGroundingState::Grounded)));
  REQUIRE(assembly.value().add_component_instance(
      ts::component("component.b", ComponentGroundingState::Free,
                    RigidTransform{Vector3{10.0, -4.0, 2.0}, Vector3{}})));

  const std::string point = reference_spelling(ConstructionPointId("construction_point.anchor"));
  auto constraint = AssemblyConstraint::create(AssemblyConstraintId("constraint.coincident"),
                                               "Coincident", AssemblyConstraintType::Coincident,
                                               local_target("component.a", point),
                                               local_target("component.b", point));
  REQUIRE(constraint);
  REQUIRE(assembly.value().add_constraint(constraint.value()));

  auto project =
      Project::create(DocumentId("project.generic.local"), "GenericLocal", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

Project cross_coincident_project(bool repeated = false) {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      ts::component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(
      ts::occurrence("subassembly.left", identity_rigid_transform())));
  if (repeated) {
    REQUIRE(root.value().add_subassembly_instance(
        ts::occurrence("subassembly.right", RigidTransform{Vector3{20.0, 0.0, 0.0}, Vector3{}})));
  }

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(
      ts::component("component.child", ComponentGroundingState::Free,
                    RigidTransform{Vector3{10.0, -4.0, 2.0}, Vector3{}})));

  auto project = Project::create(DocumentId("project.generic.cross"), "GenericCross", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(ts::solver_part()));

  const std::string point = reference_spelling(ConstructionPointId("construction_point.anchor"));
  auto left = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.left"), "Coincident left",
      AssemblyConstraintType::Coincident, hierarchy_target({}, "component.root", point),
      hierarchy_target({"subassembly.left"}, "component.child", point));
  REQUIRE(left);
  REQUIRE(project.value().add_cross_hierarchy_constraint(left.value()));

  if (repeated) {
    auto right = AssemblyHierarchyConstraint::create(
        AssemblyConstraintId("constraint.cross.right"), "Coincident right",
        AssemblyConstraintType::Coincident, hierarchy_target({}, "component.root", point),
        hierarchy_target({"subassembly.right"}, "component.child", point));
    REQUIRE(right);
    REQUIRE(project.value().add_cross_hierarchy_constraint(right.value()));
  }

  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

std::vector<ComponentInstanceId> local_group() {
  return {ComponentInstanceId("component.a"), ComponentInstanceId("component.b")};
}

const ProposedComponentTransform& local_proposal(const AssemblySolveResult& result) {
  REQUIRE(result.proposed_transforms.size() == 1U);
  return result.proposed_transforms.front();
}

RigidTransform child_transform(const Project& project) {
  return project.find_assembly_document(DocumentId("assembly.child"))
      ->find_component_instance(ComponentInstanceId("component.child"))
      ->transform();
}

} // namespace

TEST_CASE("Generic relationship equations freeze Coincident capability semantics",
          "[geometry][assembly-generic-relationships]") {
  const AssemblyGenericRelationshipEquationBuilder builder;

  auto point_point = builder.build(AssemblyConstraintId("relationship.point_point"),
                                   AssemblyConstraintType::Coincident,
                                   point_target(Point3{1.0, 2.0, 3.0}, "point.a"),
                                   point_target(Point3{4.0, 6.0, 3.0}, "point.b"));
  REQUIRE(point_point);
  const auto& point_delta =
      std::get<CoincidentPointPointResidualDescriptor>(point_point.value().residual.value);
  CHECK(point_delta.point_delta_mm == (Vector3{3.0, 4.0, 0.0}));

  auto point_line = builder.build(
      AssemblyConstraintId("relationship.point_line"), AssemblyConstraintType::Coincident,
      point_target(Point3{1.0, 2.0, 0.0}, "point.a"),
      line_target(Point3{1.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0}, "line.b"));
  REQUIRE(point_line);
  const auto& point_line_residual =
      std::get<CoincidentPointLineResidualDescriptor>(point_line.value().residual.value);
  CHECK(point_line_residual.point_target == AssemblyGenericRelationshipTargetRole::TargetA);
  CHECK(point_line_residual.point_line_cross_mm == (Vector3{0.0, 0.0, -2.0}));

  auto line_point = builder.build(
      AssemblyConstraintId("relationship.line_point"), AssemblyConstraintType::Coincident,
      line_target(Point3{1.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0}, "line.a"),
      point_target(Point3{1.0, 2.0, 0.0}, "point.b"));
  REQUIRE(line_point);
  const auto& line_point_residual =
      std::get<CoincidentPointLineResidualDescriptor>(line_point.value().residual.value);
  CHECK(line_point_residual.point_target == AssemblyGenericRelationshipTargetRole::TargetB);
  CHECK(line_point_residual.point_line_cross_mm == (Vector3{0.0, 0.0, -2.0}));

  auto point_plane = builder.build(AssemblyConstraintId("relationship.point_plane"),
                                   AssemblyConstraintType::Coincident,
                                   point_target(Point3{2.0, 3.0, 5.0}, "point.a"),
                                   plane_target(Point3{}, Vector3{0.0, 0.0, 1.0}, "plane.b"));
  REQUIRE(point_plane);
  const auto& point_plane_residual =
      std::get<CoincidentPointPlaneResidualDescriptor>(point_plane.value().residual.value);
  CHECK(point_plane_residual.point_target == AssemblyGenericRelationshipTargetRole::TargetA);
  CHECK(point_plane_residual.signed_distance_mm == Approx(5.0));

  auto plane_point = builder.build(AssemblyConstraintId("relationship.plane_point"),
                                   AssemblyConstraintType::Coincident,
                                   plane_target(Point3{}, Vector3{0.0, 0.0, 1.0}, "plane.a"),
                                   point_target(Point3{2.0, 3.0, 5.0}, "point.b"));
  REQUIRE(plane_point);
  const auto& plane_point_residual =
      std::get<CoincidentPointPlaneResidualDescriptor>(plane_point.value().residual.value);
  CHECK(plane_point_residual.point_target == AssemblyGenericRelationshipTargetRole::TargetB);
  CHECK(plane_point_residual.signed_distance_mm == Approx(5.0));
}

TEST_CASE("Generic direction equations support Line Axis and Plane capabilities",
          "[geometry][assembly-generic-relationships]") {
  const AssemblyGenericRelationshipEquationBuilder builder;

  auto parallel_line_axis =
      builder.build(AssemblyConstraintId("relationship.parallel.line_axis"),
                    AssemblyConstraintType::Parallel, line_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
                    axis_target(Point3{}, Vector3{-1.0, 0.0, 0.0}));
  REQUIRE(parallel_line_axis);
  const auto& line_axis_parallel =
      std::get<ParallelDirectionResidualDescriptor>(parallel_line_axis.value().residual.value);
  CHECK(line_axis_parallel.direction_parallelism == Vector3{});
  CHECK(parallel_line_axis.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Line);
  CHECK(parallel_line_axis.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Line);

  auto parallel_selected_axis =
      builder.build(AssemblyConstraintId("relationship.parallel.selected_axis"),
                    AssemblyConstraintType::Parallel, line_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
                    cylinder_axis_target(Point3{}, Vector3{-1.0, 0.0, 0.0}));
  REQUIRE(parallel_selected_axis);
  CHECK(parallel_selected_axis.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Line);
  CHECK(parallel_selected_axis.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Axis);
  CHECK(std::get<ParallelDirectionResidualDescriptor>(parallel_selected_axis.value().residual.value)
            .direction_parallelism == Vector3{});

  auto parallel_planes = builder.build(AssemblyConstraintId("relationship.parallel.planes"),
                                       AssemblyConstraintType::Parallel,
                                       plane_target(Point3{}, Vector3{0.0, 0.0, 1.0}),
                                       plane_target(Point3{}, Vector3{0.0, 0.0, -1.0}));
  REQUIRE(parallel_planes);
  CHECK(std::get<ParallelDirectionResidualDescriptor>(parallel_planes.value().residual.value)
            .direction_parallelism == Vector3{});

  auto perpendicular_line_axis = builder.build(
      AssemblyConstraintId("relationship.perpendicular.line_axis"),
      AssemblyConstraintType::Perpendicular, line_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
      axis_target(Point3{}, Vector3{0.0, 1.0, 0.0}));
  REQUIRE(perpendicular_line_axis);
  CHECK(std::get<PerpendicularDirectionResidualDescriptor>(
            perpendicular_line_axis.value().residual.value)
            .direction_dot == Approx(0.0));

  auto perpendicular_selected_axis = builder.build(
      AssemblyConstraintId("relationship.perpendicular.selected_axis"),
      AssemblyConstraintType::Perpendicular, cylinder_axis_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
      line_target(Point3{}, Vector3{0.0, 1.0, 0.0}));
  REQUIRE(perpendicular_selected_axis);
  CHECK(perpendicular_selected_axis.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Axis);
  CHECK(perpendicular_selected_axis.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Line);
  CHECK(std::get<PerpendicularDirectionResidualDescriptor>(
            perpendicular_selected_axis.value().residual.value)
            .direction_dot == Approx(0.0));

  auto perpendicular_planes = builder.build(
      AssemblyConstraintId("relationship.perpendicular.planes"),
      AssemblyConstraintType::Perpendicular, plane_target(Point3{}, Vector3{0.0, 0.0, 1.0}),
      plane_target(Point3{}, Vector3{1.0, 0.0, 0.0}));
  REQUIRE(perpendicular_planes);
  CHECK(std::get<PerpendicularDirectionResidualDescriptor>(
            perpendicular_planes.value().residual.value)
            .direction_dot == Approx(0.0));

  auto line_angle = builder.build(
      AssemblyConstraintId("relationship.angle.lines"), AssemblyConstraintType::Angle,
      line_target(Point3{}, Vector3{1.0, 0.0, 0.0}), line_target(Point3{}, Vector3{0.0, 1.0, 0.0}),
      angle(90.0, "relationship.angle.lines"));
  REQUIRE(line_angle);
  const auto& angle_residual =
      std::get<DirectionalAngleResidualDescriptor>(line_angle.value().residual.value);
  CHECK(angle_residual.target_angle_deg == Approx(90.0));
  CHECK(angle_residual.direction_dot == Approx(0.0));
  CHECK(angle_residual.angle_alignment == Approx(0.0).margin(1.0e-12));
  CHECK(line_angle.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Line);
  CHECK(line_angle.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Line);

  auto axis_axis_angle =
      builder.build(AssemblyConstraintId("relationship.angle.axes"), AssemblyConstraintType::Angle,
                    cylinder_axis_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
                    cylinder_axis_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
                    angle(0.0, "relationship.angle.axes"));
  REQUIRE(axis_axis_angle);
  CHECK(axis_axis_angle.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Axis);
  CHECK(axis_axis_angle.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Axis);
  CHECK(std::get<DirectionalAngleResidualDescriptor>(axis_axis_angle.value().residual.value)
            .angle_alignment == Approx(0.0));

  auto line_axis_angle =
      builder.build(AssemblyConstraintId("relationship.angle.line_axis"),
                    AssemblyConstraintType::Angle, line_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
                    cylinder_axis_target(Point3{}, Vector3{0.0, 1.0, 0.0}),
                    angle(90.0, "relationship.angle.line_axis"));
  REQUIRE(line_axis_angle);
  CHECK(line_axis_angle.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Line);
  CHECK(line_axis_angle.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Axis);
  CHECK(std::get<DirectionalAngleResidualDescriptor>(line_axis_angle.value().residual.value)
            .angle_alignment == Approx(0.0).margin(1.0e-12));

  auto axis_line_angle = builder.build(
      AssemblyConstraintId("relationship.angle.axis_line"), AssemblyConstraintType::Angle,
      cylinder_axis_target(Point3{}, Vector3{1.0, 0.0, 0.0}),
      line_target(Point3{}, Vector3{0.0, 1.0, 0.0}), angle(90.0, "relationship.angle.axis_line"));
  REQUIRE(axis_line_angle);
  CHECK(axis_line_angle.value().compatibility.target_a_capability ==
        AssemblyGeometricTargetCapability::Axis);
  CHECK(axis_line_angle.value().compatibility.target_b_capability ==
        AssemblyGeometricTargetCapability::Line);
  CHECK(std::get<DirectionalAngleResidualDescriptor>(axis_line_angle.value().residual.value)
            .angle_alignment == Approx(0.0).margin(1.0e-12));

  auto incompatible = builder.build(
      AssemblyConstraintId("relationship.coincident.bad"), AssemblyConstraintType::Coincident,
      line_target(Point3{}, Vector3{1.0, 0.0, 0.0}), line_target(Point3{}, Vector3{1.0, 0.0, 0.0}));
  CHECK(incompatible.has_error());
}

TEST_CASE("Local generic relationships enter the shared solve freshness and rank paths",
          "[geometry][assembly-generic-relationships-solver]"
          "[geometry][assembly-generic-relationships-diagnostics]") {
  Project project = local_coincident_project();
  auto graph = AssemblyConstraintGraph::build(project.assembly());
  REQUIRE(graph);
  CHECK(graph.value().edge_count() == 1U);

  const RigidTransform source =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
  const AssemblyRigidBodySolver solver;
  auto solved = solver.solve(project, local_group());
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  CHECK(solved.value().residual_summary.residual_component_count == 3U);
  CHECK(solved.value().residual_summary.final_rms <= 1.0e-8);
  const auto& proposal = local_proposal(solved.value());
  CHECK(proposal.source_transform == source);
  CHECK_FALSE(proposal.proposed_transform == source);
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      source);

  Project applied_project = project;
  const AssemblySolveResultApplier local_applier;
  auto local_applied = local_applier.apply(applied_project, solved.value());
  REQUIRE(local_applied);
  CHECK(local_applied.value() == 1U);
  auto verified = solver.solve(applied_project, local_group());
  REQUIRE(verified);
  REQUIRE(verified.value().converged());
  CHECK(verified.value().residual_summary.initial_rms <= 1.0e-8);

  const AssemblySolveDiagnosticsAnalyzer diagnostics_analyzer;
  auto diagnostics = diagnostics_analyzer.analyze(project, local_group());
  REQUIRE(diagnostics);
  REQUIRE(diagnostics.value().rank_summary.rank_evaluated);
  CHECK(diagnostics.value().rank_summary.jacobian_rank == 3U);
  CHECK(diagnostics.value().rank_summary.constrained_dof == 3U);
  CHECK(diagnostics.value().rank_summary.remaining_dof == 3U);
  CHECK(diagnostics.value().dof_classification == AssemblyDofClassification::Underconstrained);

  auto thickness = Quantity::length_mm(9.0, "part.thickness");
  REQUIRE(thickness);
  REQUIRE(project.find_part_document(DocumentId("part.block27_plate"))
              ->set_parameter_value(ParameterId("part.thickness"), thickness.value()));
  const AssemblySolveResultApplier applier;
  CHECK(applier.apply(project, solved.value()).has_error());
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      source);
}

TEST_CASE(
    "Cross-hierarchy generic relationships reuse authorities solver application and diagnostics",
    "[geometry][assembly-generic-relationships-cross-hierarchy]"
    "[geometry][assembly-generic-relationships-diagnostics]") {
  Project project = cross_coincident_project();
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(graph);
  REQUIRE(graph.value().solve_group_count() == 1U);
  CHECK(graph.value().relationship_count() == 1U);
  CHECK(graph.value().authority_count() == 2U);

  const RigidTransform source = child_transform(project);
  const AssemblyCrossHierarchyConstraintSolver solver;
  auto solved = solver.solve(project, graph.value().solve_groups().front());
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  CHECK(solved.value().residual_summary.residual_component_count == 3U);
  REQUIRE(solved.value().proposed_transforms.size() == 1U);
  CHECK(solved.value().proposed_transforms.front().source_transform == source);
  CHECK_FALSE(solved.value().proposed_transforms.front().proposed_transform == source);
  CHECK(child_transform(project) == source);

  const AssemblyCrossHierarchySolveDiagnosticsAnalyzer diagnostics_analyzer;
  auto diagnostics = diagnostics_analyzer.analyze(project, graph.value().solve_groups().front());
  REQUIRE(diagnostics);
  REQUIRE(diagnostics.value().rank_summary.rank_evaluated);
  CHECK(diagnostics.value().rank_summary.jacobian_rank == 3U);
  CHECK(diagnostics.value().rank_summary.constrained_dof == 3U);
  CHECK(diagnostics.value().rank_summary.remaining_dof == 3U);

  const AssemblyCrossHierarchySolveResultApplier applier;
  auto applied = applier.apply(project, solved.value());
  REQUIRE(applied);
  CHECK(applied.value() == 1U);
  auto verified = solver.solve(project, graph.value().solve_groups().front());
  REQUIRE(verified);
  REQUIRE(verified.value().converged());
  CHECK(verified.value().residual_summary.initial_rms <= 1.0e-8);
}

TEST_CASE("Repeated generic endpoints preserve one shared child transform authority",
          "[geometry][assembly-generic-relationships-cross-hierarchy]") {
  const Project project = cross_coincident_project(true);
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(graph);
  CHECK(graph.value().relationship_count() == 2U);
  CHECK(graph.value().authority_count() == 2U);
  REQUIRE(graph.value().solve_group_count() == 1U);
  CHECK(graph.value().solve_groups().front().relationships.size() == 2U);
  CHECK(graph.value().solve_groups().front().authorities.size() == 2U);
  const ComponentTransformAuthority child_authority{DocumentId("assembly.child"),
                                                    ComponentInstanceId("component.child")};
  CHECK(graph.value().solve_groups().front().authorities[0] == child_authority);
  const ComponentTransformAuthority root_authority{DocumentId("assembly.root"),
                                                   ComponentInstanceId("component.root")};
  CHECK(graph.value().solve_groups().front().authorities[1] == root_authority);
}

TEST_CASE("Cross-hierarchy generic solve results stale on semantic target part edits",
          "[geometry][assembly-generic-relationships-cross-hierarchy]") {
  Project project = cross_coincident_project();
  const auto group = ts::only_group(project);
  const AssemblyCrossHierarchyConstraintSolver solver;
  auto solved = solver.solve(project, group);
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  const RigidTransform source = child_transform(project);

  auto thickness = Quantity::length_mm(9.0, "part.thickness");
  REQUIRE(thickness);
  REQUIRE(project.find_part_document(DocumentId("part.block27_plate"))
              ->set_parameter_value(ParameterId("part.thickness"), thickness.value()));

  const AssemblyCrossHierarchySolveResultApplier applier;
  CHECK(applier.apply(project, solved.value()).has_error());
  CHECK(child_transform(project) == source);
}
