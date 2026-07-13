#pragma once

#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

namespace blcad::geometry::test_support {

inline Parameter length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

inline PartDocument solver_part(Point3 reference_anchor = Point3{1.0, 2.0, 3.0},
                                Vector3 reference_line_direction = Vector3{0.0, 1.0, 0.0},
                                Point3 reference_axis_origin = Point3{-2.0, 5.0, 7.0},
                                Vector3 reference_axis_direction = Vector3{0.0, 0.0, 1.0}) {
  auto part = PartDocument::create(DocumentId("part.block27_plate"), "Block27Plate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.height", "height", 80.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(
      part.value().add_parameter(length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));

  auto anchor = ConstructionPoint::create_explicit(ConstructionPointId("construction_point.anchor"),
                                                   "Anchor", reference_anchor);
  REQUIRE(anchor);
  REQUIRE(part.value().add_construction_point(anchor.value()));
  auto spherical_center =
      ConstructionPoint::create_explicit(ConstructionPointId("construction_point.spherical_center"),
                                         "SphericalCenter", Point3{4.0, -6.0, 0.0});
  REQUIRE(spherical_center);
  REQUIRE(part.value().add_construction_point(spherical_center.value()));

  auto line =
      ConstructionLine::create_explicit(ConstructionLineId("construction_line.axis_y"), "AxisY",
                                        reference_anchor, reference_line_direction);
  REQUIRE(line);
  REQUIRE(part.value().add_construction_line(line.value()));

  auto explicit_axis = DatumAxis::create_explicit(DatumAxisId("datum_axis.explicit_z"), "ExplicitZ",
                                                  reference_axis_origin, reference_axis_direction);
  REQUIRE(explicit_axis);
  REQUIRE(part.value().add_datum_axis(explicit_axis.value()));

  auto derived_axis =
      DatumAxis::create_from_construction_line(DatumAxisId("datum_axis.from_line"), "FromLine",
                                               ConstructionLineId("construction_line.axis_y"));
  REQUIRE(derived_axis);
  REQUIRE(part.value().add_datum_axis(derived_axis.value()));

  auto base = Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(base);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(base.value()));

  auto base_feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base_feature);
  REQUIRE(part.value().add_feature(base_feature.value()));

  auto hole = Sketch::create(SketchId("sketch.hole"), "HoleSketch", DatumPlaneId("datum.xy"));
  REQUIRE(hole);
  auto circle = CircleProfile::create(ProfileId("profile.hole"), ParameterId("part.hole_diameter"),
                                      Point2{4.0, -6.0});
  REQUIRE(circle);
  REQUIRE(hole.value().add_profile(circle.value()));
  REQUIRE(part.value().add_sketch(hole.value()));

  auto hole_feature = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"), FeatureId("feature.base_extrude"),
      SubtractiveExtrudeDepth::ThroughAll, ExtrudeDirection::SketchNormal);
  REQUIRE(hole_feature);
  REQUIRE(part.value().add_feature(hole_feature.value()));
  return part.value();
}

inline ComponentInstance
component(const char* id, ComponentGroundingState grounding,
          RigidTransform transform = identity_rigid_transform(),
          ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto instance =
      ComponentInstance::create(ComponentInstanceId(id), id, DocumentId("part.block27_plate"),
                                ComponentVisibility::Visible, suppression, grounding, transform);
  REQUIRE(instance);
  return instance.value();
}

inline SubassemblyInstance
occurrence(const char* id, RigidTransform transform,
           ComponentVisibility visibility = ComponentVisibility::Visible,
           ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto instance =
      SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId("assembly.child"),
                                  visibility, suppression, transform);
  REQUIRE(instance);
  return instance.value();
}

inline AssemblyHierarchyConstraintEndpoint endpoint(std::initializer_list<const char*> path,
                                                    const char* component_id,
                                                    const char* semantic_reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const char* id : path) {
    occurrence_path.emplace_back(id);
  }
  auto value = AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path), ComponentInstanceId(component_id), semantic_reference);
  REQUIRE(value);
  return value.value();
}

inline Quantity distance(double value_mm, const char* id) {
  auto value = Quantity::length_mm(value_mm, id);
  REQUIRE(value);
  return value.value();
}

inline AssemblyHierarchyConstraint
distance_constraint(const char* id, double target_mm,
                    const char* child_reference = "feature.base_extrude.top",
                    const char* root_reference = "feature.base_extrude.top",
                    std::initializer_list<const char*> child_path = {"subassembly.left"}) {
  auto constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Distance,
      endpoint({}, "component.root", root_reference),
      endpoint(child_path, "component.child", child_reference), AssemblyConstraintState::Active,
      distance(target_mm, id));
  REQUIRE(constraint);
  return constraint.value();
}

inline Project distance_project(double target_mm = 20.0, bool repeated = false,
                                bool add_second_relationship = false,
                                const char* child_reference = "feature.base_extrude.top") {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(
      occurrence("subassembly.left", identity_rigid_transform())));
  if (repeated) {
    REQUIRE(root.value().add_subassembly_instance(
        occurrence("subassembly.right", RigidTransform{Vector3{0.0, 0.0, 10.0}, Vector3{}})));
  }

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(
      component("component.child", ComponentGroundingState::Free)));

  auto project = Project::create(DocumentId("project.block27"), "Block27", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(solver_part()));
  REQUIRE(project.value().add_cross_hierarchy_constraint(
      distance_constraint("constraint.cross.anchor", target_mm, child_reference)));
  if (add_second_relationship) {
    REQUIRE(project.value().add_cross_hierarchy_constraint(
        distance_constraint("constraint.cross.second", target_mm + 5.0, child_reference)));
  }
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

inline Project repeated_occurrence_project(bool reverse_relationship_insertion = false) {
  Project project = distance_project(20.0, true, false);
  auto same_authority = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.same_authority"), "same_authority",
      AssemblyConstraintType::Distance,
      endpoint({"subassembly.left"}, "component.child", "feature.base_extrude.top"),
      endpoint({"subassembly.right"}, "component.child", "feature.base_extrude.top"),
      AssemblyConstraintState::Active, distance(10.0, "constraint.cross.same_authority"));
  REQUIRE(same_authority);

  if (!reverse_relationship_insertion) {
    REQUIRE(project.add_cross_hierarchy_constraint(same_authority.value()));
    REQUIRE(project.validate_assembly_structure());
    return project;
  }

  Project reverse = distance_project(20.0, true, false);
  AssemblyHierarchyConstraint anchor = reverse.cross_hierarchy_constraints().front();
  reverse.cross_hierarchy_constraints().clear();
  REQUIRE(reverse.add_cross_hierarchy_constraint(same_authority.value()));
  REQUIRE(reverse.add_cross_hierarchy_constraint(std::move(anchor)));
  REQUIRE(reverse.validate_assembly_structure());
  return reverse;
}

inline AssemblyCrossHierarchySolveGroup only_group(const Project& project) {
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(graph);
  REQUIRE(graph.value().solve_group_count() == 1U);
  return graph.value().solve_groups().front();
}

inline AssemblyCrossHierarchySolveResult solve_only_group(const Project& project) {
  const AssemblyCrossHierarchyConstraintSolver solver;
  auto result = solver.solve(project, only_group(project));
  REQUIRE(result);
  return result.value();
}

} // namespace blcad::geometry::test_support
