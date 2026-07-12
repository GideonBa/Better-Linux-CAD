#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/generated_topology_reference.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_concentric_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"
#include "blcad/geometry/assembly_target_compatibility.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
namespace ts = blcad::geometry::test_support;

namespace {

constexpr const char* kPartDocument = "part.block27_plate";
constexpr const char* kHoleFeature = "feature.hole";
constexpr const char* kHoleProfile = "profile.hole";
constexpr const char* kCompatibilityObjectId = "geometry.assembly_target_compatibility";

AssemblyResolvedGeometricTarget synthetic_target(AssemblyGeometricTargetSourceKind kind,
                                                 AssemblyGeometricTargetDescriptor descriptor) {
  AssemblyGeometricTargetSourceMetadata metadata;
  metadata.referenced_part_document = DocumentId("part.synthetic");
  switch (kind) {
  case AssemblyGeometricTargetSourceKind::GeneratedPlanarFace:
    metadata.source_feature = FeatureId("feature.synthetic");
    metadata.semantic_face = SemanticFace::Top;
    break;
  case AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace:
    metadata.source_feature = FeatureId("feature.synthetic");
    metadata.source_profile = ProfileId("profile.synthetic");
    break;
  case AssemblyGeometricTargetSourceKind::GeneratedLinearEdge:
  case AssemblyGeometricTargetSourceKind::GeneratedCircularEdge:
  case AssemblyGeometricTargetSourceKind::GeneratedVertex:
    metadata.source_feature = FeatureId("feature.synthetic");
    break;
  case AssemblyGeometricTargetSourceKind::CircularFeatureSeat:
    metadata.source_feature = FeatureId("feature.synthetic");
    metadata.source_profile = ProfileId("profile.synthetic");
    metadata.semantic_axis = SemanticAxis::Primary;
    metadata.semantic_seating_plane = SemanticSeatingPlane::Primary;
    break;
  case AssemblyGeometricTargetSourceKind::DatumPlane:
  case AssemblyGeometricTargetSourceKind::DatumAxis:
  case AssemblyGeometricTargetSourceKind::ConstructionLine:
  case AssemblyGeometricTargetSourceKind::ConstructionPoint:
    break;
  }

  return AssemblyResolvedGeometricTarget{
      AssemblyLocalGeometricTargetEndpointIdentity{ComponentInstanceId("component.synthetic"),
                                                   "synthetic.source"},
      kind,
      std::move(metadata),
      std::move(descriptor),
      assembly_geometric_target_capabilities(kind),
      AssemblyGeometricTargetCoordinateSpace::ComponentLocal,
      {identity_rigid_transform()}};
}

AssemblyResolvedGeometricTarget plane_target() {
  return synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedPlanarFace,
                          AssemblyPlanarTargetDescriptor{Point3{}, Vector3{1.0, 0.0, 0.0},
                                                         Vector3{0.0, 1.0, 0.0},
                                                         Vector3{0.0, 0.0, 1.0}});
}

AssemblyResolvedGeometricTarget axis_target() {
  return synthetic_target(
      AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace,
      AssemblyCylindricalSurfaceTargetDescriptor{Point3{}, Vector3{0.0, 0.0, 1.0}, 5.0});
}

AssemblyResolvedGeometricTarget datum_axis_target() {
  return synthetic_target(AssemblyGeometricTargetSourceKind::DatumAxis,
                          AssemblyAxisTargetDescriptor{Point3{}, Vector3{0.0, 0.0, 1.0}});
}

AssemblyResolvedGeometricTarget line_target() {
  return synthetic_target(AssemblyGeometricTargetSourceKind::ConstructionLine,
                          AssemblyLineTargetDescriptor{Point3{}, Vector3{1.0, 0.0, 0.0}});
}

AssemblyResolvedGeometricTarget point_target() {
  return synthetic_target(AssemblyGeometricTargetSourceKind::ConstructionPoint,
                          AssemblyPointTargetDescriptor{Point3{1.0, 2.0, 3.0}});
}

AssemblyResolvedGeometricTarget circular_edge_target() {
  return synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedCircularEdge,
                          AssemblyCircularEdgeTargetDescriptor{Point3{}, Vector3{1.0, 0.0, 0.0},
                                                               Vector3{0.0, 1.0, 0.0},
                                                               Vector3{0.0, 0.0, 1.0}, 10.0});
}

AssemblyResolvedGeometricTarget frame_target() {
  return synthetic_target(AssemblyGeometricTargetSourceKind::CircularFeatureSeat,
                          AssemblyFrameTargetDescriptor{Point3{}, Vector3{1.0, 0.0, 0.0},
                                                        Vector3{0.0, 1.0, 0.0},
                                                        Vector3{0.0, 0.0, 1.0}});
}

void check_compatible(AssemblyConstraintType relationship_type,
                      const AssemblyResolvedGeometricTarget& target_a,
                      const AssemblyResolvedGeometricTarget& target_b,
                      AssemblyGeometricTargetCapability capability_a,
                      AssemblyGeometricTargetCapability capability_b) {
  const AssemblyTargetCompatibilityResolver resolver;
  const auto compatibility = resolver.resolve(relationship_type, target_a, target_b);
  REQUIRE(compatibility);
  CHECK(compatibility.value().relationship_type == relationship_type);
  CHECK(compatibility.value().target_a_capability == capability_a);
  CHECK(compatibility.value().target_b_capability == capability_b);
}

void check_incompatible(AssemblyConstraintType relationship_type,
                        const AssemblyResolvedGeometricTarget& target_a,
                        const AssemblyResolvedGeometricTarget& target_b) {
  const AssemblyTargetCompatibilityResolver resolver;
  const auto compatibility = resolver.resolve(relationship_type, target_a, target_b);
  REQUIRE(compatibility.has_error());
  CHECK(compatibility.error().object_id() == kCompatibilityObjectId);
}

ComponentInstance component(const char* id, RigidTransform transform = identity_rigid_transform()) {
  auto value = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId(kPartDocument), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, transform);
  REQUIRE(value);
  return value.value();
}

Project local_project() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.compatibility"), "Compatibility");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId(kPartDocument)));
  REQUIRE(assembly.value().add_component_instance(component("component.a")));
  REQUIRE(assembly.value().add_component_instance(component("component.b")));

  auto project =
      Project::create(DocumentId("project.compatibility"), "Compatibility", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

AssemblyConstraintTarget local_target(const char* component_id,
                                      const std::string& semantic_reference) {
  auto target =
      AssemblyConstraintTarget::create(ComponentInstanceId(component_id), semantic_reference);
  REQUIRE(target);
  return target.value();
}

AssemblyConstraint constraint(const char* id, AssemblyConstraintType type, const char* component_a,
                              const std::string& target_a, const char* component_b,
                              const std::string& target_b) {
  auto value = AssemblyConstraint::create(AssemblyConstraintId(id), id, type,
                                          local_target(component_a, target_a),
                                          local_target(component_b, target_b));
  REQUIRE(value);
  return value.value();
}

AssemblyHierarchyConstraintTarget hierarchy_target(std::initializer_list<const char*> path,
                                                   const char* component_id,
                                                   const std::string& semantic_reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const char* id : path) {
    occurrence_path.emplace_back(id);
  }
  auto target = AssemblyHierarchyConstraintTarget::create(
      std::move(occurrence_path), ComponentInstanceId(component_id), semantic_reference);
  REQUIRE(target);
  return target.value();
}

template <typename Id> std::string reference_spelling(Id id) {
  auto spelling = make_assembly_reference_target_spelling(AssemblyReferenceTargetIdentity{id});
  REQUIRE(spelling);
  return spelling.value();
}

std::string topo_spelling(GeneratedTopologyReferenceIdentity identity) {
  auto spelling = make_generated_topology_target_spelling(identity);
  REQUIRE(spelling);
  return spelling.value();
}

std::string cylindrical_face_spelling() {
  auto reference = SemanticCylindricalFaceReference::create(
      FeatureId(kHoleFeature), ProfileId(kHoleProfile), SemanticCylindricalFace::Wall);
  REQUIRE(reference);
  return topo_spelling(GeneratedTopologyReferenceIdentity{std::move(reference.value())});
}

std::string circular_edge_spelling(SemanticCircularEdge edge) {
  auto reference =
      SemanticCircularEdgeReference::create(FeatureId(kHoleFeature), ProfileId(kHoleProfile), edge);
  REQUIRE(reference);
  return topo_spelling(GeneratedTopologyReferenceIdentity{std::move(reference.value())});
}

AssemblyTargetCompatibility local_compatibility(const Project& project,
                                                AssemblyConstraintType relationship_type,
                                                const AssemblyConstraintTarget& target_a,
                                                const AssemblyConstraintTarget& target_b) {
  const AssemblyConstraintTargetResolver target_resolver;
  const AssemblyTargetCompatibilityResolver compatibility_resolver;
  auto resolved_a = target_resolver.resolve_geometric(project, target_a);
  auto resolved_b = target_resolver.resolve_geometric(project, target_b);
  REQUIRE(resolved_a);
  REQUIRE(resolved_b);
  auto compatibility =
      compatibility_resolver.resolve(relationship_type, resolved_a.value(), resolved_b.value());
  REQUIRE(compatibility);
  return compatibility.value();
}

AssemblyTargetCompatibility
hierarchy_compatibility(const Project& project, AssemblyConstraintType relationship_type,
                        const AssemblyHierarchyConstraintTarget& target_a,
                        const AssemblyHierarchyConstraintTarget& target_b) {
  const AssemblyHierarchyConstraintTargetResolver target_resolver;
  const AssemblyTargetCompatibilityResolver compatibility_resolver;
  auto resolved_a = target_resolver.resolve_geometric(project, target_a);
  auto resolved_b = target_resolver.resolve_geometric(project, target_b);
  REQUIRE(resolved_a);
  REQUIRE(resolved_b);
  auto compatibility =
      compatibility_resolver.resolve(relationship_type, resolved_a.value(), resolved_b.value());
  REQUIRE(compatibility);
  return compatibility.value();
}

} // namespace

TEST_CASE("Assembly target compatibility resolver covers the supported relationship matrix",
          "[geometry][assembly-target-compatibility]") {
  check_compatible(AssemblyConstraintType::Mate, plane_target(), plane_target(),
                   AssemblyGeometricTargetCapability::Plane,
                   AssemblyGeometricTargetCapability::Plane);

  check_compatible(AssemblyConstraintType::Distance, plane_target(), plane_target(),
                   AssemblyGeometricTargetCapability::Plane,
                   AssemblyGeometricTargetCapability::Plane);
  check_compatible(AssemblyConstraintType::Distance, point_target(), point_target(),
                   AssemblyGeometricTargetCapability::Point,
                   AssemblyGeometricTargetCapability::Point);
  check_compatible(AssemblyConstraintType::Distance, point_target(), plane_target(),
                   AssemblyGeometricTargetCapability::Point,
                   AssemblyGeometricTargetCapability::Plane);
  check_compatible(AssemblyConstraintType::Distance, plane_target(), point_target(),
                   AssemblyGeometricTargetCapability::Plane,
                   AssemblyGeometricTargetCapability::Point);

  check_compatible(AssemblyConstraintType::Angle, plane_target(), plane_target(),
                   AssemblyGeometricTargetCapability::Plane,
                   AssemblyGeometricTargetCapability::Plane);
  check_compatible(AssemblyConstraintType::Angle, line_target(), line_target(),
                   AssemblyGeometricTargetCapability::Line,
                   AssemblyGeometricTargetCapability::Line);
  check_compatible(AssemblyConstraintType::Angle, axis_target(), axis_target(),
                   AssemblyGeometricTargetCapability::Axis,
                   AssemblyGeometricTargetCapability::Axis);
  check_compatible(AssemblyConstraintType::Angle, line_target(), axis_target(),
                   AssemblyGeometricTargetCapability::Line,
                   AssemblyGeometricTargetCapability::Axis);
  check_compatible(AssemblyConstraintType::Angle, axis_target(), line_target(),
                   AssemblyGeometricTargetCapability::Axis,
                   AssemblyGeometricTargetCapability::Line);

  check_compatible(AssemblyConstraintType::Concentric, axis_target(), axis_target(),
                   AssemblyGeometricTargetCapability::Axis,
                   AssemblyGeometricTargetCapability::Axis);
  check_compatible(AssemblyConstraintType::Insert, frame_target(), frame_target(),
                   AssemblyGeometricTargetCapability::Frame,
                   AssemblyGeometricTargetCapability::Frame);
}

TEST_CASE("Assembly target compatibility resolver fails closed for incompatible pairs",
          "[geometry][assembly-target-compatibility]") {
  check_incompatible(AssemblyConstraintType::Mate, point_target(), plane_target());
  check_incompatible(AssemblyConstraintType::Distance, line_target(), line_target());
  check_incompatible(AssemblyConstraintType::Angle, point_target(), point_target());
  check_incompatible(AssemblyConstraintType::Concentric, line_target(), axis_target());
  check_incompatible(AssemblyConstraintType::Insert, axis_target(), frame_target());
}

TEST_CASE("Assembly target compatibility resolver is deterministic for multi-capability targets",
          "[geometry][assembly-target-compatibility]") {
  check_compatible(AssemblyConstraintType::Mate, frame_target(), plane_target(),
                   AssemblyGeometricTargetCapability::Plane,
                   AssemblyGeometricTargetCapability::Plane);
  check_compatible(AssemblyConstraintType::Distance, circular_edge_target(), point_target(),
                   AssemblyGeometricTargetCapability::Point,
                   AssemblyGeometricTargetCapability::Point);
  check_compatible(AssemblyConstraintType::Angle, datum_axis_target(), datum_axis_target(),
                   AssemblyGeometricTargetCapability::Line,
                   AssemblyGeometricTargetCapability::Line);
  check_compatible(AssemblyConstraintType::Angle, circular_edge_target(), datum_axis_target(),
                   AssemblyGeometricTargetCapability::Axis,
                   AssemblyGeometricTargetCapability::Axis);
  check_compatible(AssemblyConstraintType::Concentric, frame_target(), datum_axis_target(),
                   AssemblyGeometricTargetCapability::Axis,
                   AssemblyGeometricTargetCapability::Axis);
  check_compatible(AssemblyConstraintType::Insert, frame_target(), frame_target(),
                   AssemblyGeometricTargetCapability::Frame,
                   AssemblyGeometricTargetCapability::Frame);
}

TEST_CASE("Assembly target compatibility resolves current local target families without mutation",
          "[geometry][assembly-target-compatibility]") {
  Project project = local_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);

  const std::string datum_plane = reference_spelling(DatumPlaneId("datum.xy"));
  const std::string datum_axis = reference_spelling(DatumAxisId("datum_axis.explicit_z"));
  const std::string cylinder = cylindrical_face_spelling();
  const std::string source_rim = circular_edge_spelling(SemanticCircularEdge::SourceRim);

  auto mate = local_compatibility(project, AssemblyConstraintType::Mate,
                                  local_target("component.a", datum_plane),
                                  local_target("component.b", "feature.base_extrude.top"));
  CHECK(mate.target_a_capability == AssemblyGeometricTargetCapability::Plane);
  CHECK(mate.target_b_capability == AssemblyGeometricTargetCapability::Plane);

  auto datum_to_cylinder = local_compatibility(project, AssemblyConstraintType::Concentric,
                                               local_target("component.a", datum_axis),
                                               local_target("component.b", cylinder));
  CHECK(datum_to_cylinder.target_a_capability == AssemblyGeometricTargetCapability::Axis);
  CHECK(datum_to_cylinder.target_b_capability == AssemblyGeometricTargetCapability::Axis);

  auto rim_to_datum = local_compatibility(project, AssemblyConstraintType::Concentric,
                                          local_target("component.a", source_rim),
                                          local_target("component.b", datum_axis));
  CHECK(rim_to_datum.target_a_capability == AssemblyGeometricTargetCapability::Axis);
  CHECK(rim_to_datum.target_b_capability == AssemblyGeometricTargetCapability::Axis);

  auto insert = local_compatibility(project, AssemblyConstraintType::Insert,
                                    local_target("component.a", "feature.hole.seat"),
                                    local_target("component.b", "feature.hole.seat"));
  CHECK(insert.target_a_capability == AssemblyGeometricTargetCapability::Frame);
  CHECK(insert.target_b_capability == AssemblyGeometricTargetCapability::Frame);

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Assembly equation builders consume compatibility before projection",
          "[geometry][assembly-target-compatibility]") {
  Project project = local_project();
  const std::string datum_plane = reference_spelling(DatumPlaneId("datum.xy"));
  const std::string datum_axis = reference_spelling(DatumAxisId("datum_axis.explicit_z"));
  const std::string cylinder = cylindrical_face_spelling();
  const std::string source_rim = circular_edge_spelling(SemanticCircularEdge::SourceRim);

  const AssemblyConstraintEquationBuilder planar_builder;
  const auto mate_equation = planar_builder.build(
      project, constraint("constraint.mate.reference", AssemblyConstraintType::Mate, "component.a",
                          datum_plane, "component.b", "feature.base_extrude.top"));
  REQUIRE(mate_equation);

  const AssemblyConcentricConstraintEquationBuilder concentric_builder;
  const auto datum_to_cylinder = concentric_builder.build(
      project,
      constraint("constraint.concentric.datum_cylinder", AssemblyConstraintType::Concentric,
                 "component.a", datum_axis, "component.b", cylinder));
  REQUIRE(datum_to_cylinder);

  const auto rim_to_datum = concentric_builder.build(
      project, constraint("constraint.concentric.rim_datum", AssemblyConstraintType::Concentric,
                          "component.a", source_rim, "component.b", datum_axis));
  REQUIRE(rim_to_datum);

  const AssemblyInsertConstraintEquationBuilder insert_builder;
  const auto insert_equation = insert_builder.build(
      project, constraint("constraint.insert.seat", AssemblyConstraintType::Insert, "component.a",
                          "feature.hole.seat", "component.b", "feature.hole.seat"));
  REQUIRE(insert_equation);

  const auto incompatible = concentric_builder.build(
      project, constraint("constraint.concentric.incompatible", AssemblyConstraintType::Concentric,
                          "component.a", datum_plane, "component.b", cylinder));
  REQUIRE(incompatible.has_error());
  CHECK(incompatible.error().object_id() == kCompatibilityObjectId);
}

TEST_CASE("Local and cross-hierarchy target compatibility agree for the same semantic families",
          "[geometry][assembly-target-compatibility]"
          "[geometry][assembly-cross-hierarchy-target-compatibility]") {
  Project local = local_project();
  Project hierarchy = ts::distance_project();
  const std::string datum_axis = reference_spelling(DatumAxisId("datum_axis.explicit_z"));
  const std::string source_rim = circular_edge_spelling(SemanticCircularEdge::SourceRim);

  const AssemblyTargetCompatibility local_result = local_compatibility(
      local, AssemblyConstraintType::Concentric, local_target("component.a", source_rim),
      local_target("component.b", datum_axis));
  const AssemblyTargetCompatibility hierarchy_result = hierarchy_compatibility(
      hierarchy, AssemblyConstraintType::Concentric,
      hierarchy_target({}, "component.root", source_rim),
      hierarchy_target({"subassembly.left"}, "component.child", datum_axis));

  CHECK(hierarchy_result == local_result);
}

TEST_CASE("Cross-hierarchy equation builder consumes target compatibility",
          "[geometry][assembly-cross-hierarchy-target-compatibility]") {
  Project project = ts::distance_project();
  const std::string datum_plane = reference_spelling(DatumPlaneId("datum.xy"));
  const std::string datum_axis = reference_spelling(DatumAxisId("datum_axis.explicit_z"));
  const std::string cylinder = cylindrical_face_spelling();
  const std::string source_rim = circular_edge_spelling(SemanticCircularEdge::SourceRim);

  const AssemblyHierarchyConstraintEquationBuilder builder;
  auto mate_query = AssemblyHierarchyConstraintQuery::create(
      AssemblyConstraintId("query.mate.reference"), AssemblyConstraintType::Mate,
      hierarchy_target({}, "component.root", datum_plane),
      hierarchy_target({"subassembly.left"}, "component.child", "feature.base_extrude.top"));
  REQUIRE(mate_query);
  REQUIRE(builder.build(project, mate_query.value()));

  auto concentric_query = AssemblyHierarchyConstraintQuery::create(
      AssemblyConstraintId("query.concentric.rim_datum"), AssemblyConstraintType::Concentric,
      hierarchy_target({}, "component.root", source_rim),
      hierarchy_target({"subassembly.left"}, "component.child", datum_axis));
  REQUIRE(concentric_query);
  REQUIRE(builder.build(project, concentric_query.value()));

  auto cylinder_query = AssemblyHierarchyConstraintQuery::create(
      AssemblyConstraintId("query.concentric.datum_cylinder"), AssemblyConstraintType::Concentric,
      hierarchy_target({}, "component.root", datum_axis),
      hierarchy_target({"subassembly.left"}, "component.child", cylinder));
  REQUIRE(cylinder_query);
  REQUIRE(builder.build(project, cylinder_query.value()));

  auto incompatible_query = AssemblyHierarchyConstraintQuery::create(
      AssemblyConstraintId("query.concentric.incompatible"), AssemblyConstraintType::Concentric,
      hierarchy_target({}, "component.root", datum_plane),
      hierarchy_target({"subassembly.left"}, "component.child", cylinder));
  REQUIRE(incompatible_query);
  const auto incompatible = builder.build(project, incompatible_query.value());
  REQUIRE(incompatible.has_error());
  CHECK(incompatible.error().object_id() == kCompatibilityObjectId);
}
