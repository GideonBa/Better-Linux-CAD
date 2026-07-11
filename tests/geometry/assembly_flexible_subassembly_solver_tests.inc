#include "blcad/geometry/assembly_flexible_subassembly_solver.hpp"

#include "blcad/core/assembly_leaf_occurrence.hpp"

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <vector>

namespace {

constexpr double kFlexibleSubassemblySolveTolerance = 1.0e-6;

Parameter make_flexible_subassembly_length_parameter(const char* id, const char* name,
                                                     double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_flexible_subassembly_face_part() {
  auto part = PartDocument::create(DocumentId("part.face_plate"), "FacePlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(
      make_flexible_subassembly_length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(
      make_flexible_subassembly_length_parameter("part.height", "height", 80.0)));
  REQUIRE(part.value().add_parameter(
      make_flexible_subassembly_length_parameter("part.thickness", "thickness", 8.0)));

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

AssemblyConstraintTarget make_flexible_subassembly_target(const char* component,
                                                          const char* semantic_reference) {
  auto target =
      AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

AssemblyDocument make_flexible_child_assembly() {
  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "ChildAssembly");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.face_plate")));

  auto fixed = ComponentInstance::create(
      ComponentInstanceId("component.a"), "Fixed", DocumentId("part.face_plate"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Grounded, identity_rigid_transform());
  REQUIRE(fixed);
  REQUIRE(child.value().add_component_instance(fixed.value()));

  auto moving = ComponentInstance::create(
      ComponentInstanceId("component.b"), "Moving", DocumentId("part.face_plate"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Free,
      RigidTransform{Vector3{5.0, -3.0, 20.0}, Vector3{}});
  REQUIRE(moving);
  REQUIRE(child.value().add_component_instance(moving.value()));

  auto mate = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.mate"), "Mate", AssemblyConstraintType::Mate,
      make_flexible_subassembly_target("component.a", "feature.base_extrude.top"),
      make_flexible_subassembly_target("component.b", "feature.base_extrude.bottom"),
      AssemblyConstraintState::Active, std::nullopt);
  REQUIRE(mate);
  REQUIRE(child.value().add_constraint(mate.value()));
  return child.value();
}

SubassemblyInstance make_flexible_subassembly_instance(
    const char* id, double x,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto instance = SubassemblyInstance::create(
      SubassemblyInstanceId(id), id, DocumentId("assembly.child"), ComponentVisibility::Visible,
      suppression, RigidTransform{Vector3{x, 0.0, 0.0}, Vector3{}});
  REQUIRE(instance);
  return instance.value();
}

Project make_flexible_subassembly_project(
    bool repeated = false,
    ComponentSuppressionState first_suppression = ComponentSuppressionState::Active) {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "RootAssembly");
  REQUIRE(root);
  REQUIRE(root.value().add_subassembly_instance(
      make_flexible_subassembly_instance("subassembly.left", 100.0, first_suppression)));
  if (repeated) {
    REQUIRE(root.value().add_subassembly_instance(
        make_flexible_subassembly_instance("subassembly.right", -100.0)));
  }

  auto project = Project::create(DocumentId("project.flexible"), "FlexibleProject", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(make_flexible_child_assembly()));
  REQUIRE(project.value().add_part_document(make_flexible_subassembly_face_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

std::vector<ComponentInstanceId>
flexible_subassembly_group(std::initializer_list<const char*> ids) {
  std::vector<ComponentInstanceId> result;
  result.reserve(ids.size());
  for (const char* id : ids) {
    result.emplace_back(id);
  }
  return result;
}

std::vector<SubassemblyInstanceId>
flexible_subassembly_path(std::initializer_list<const char*> ids) {
  std::vector<SubassemblyInstanceId> result;
  result.reserve(ids.size());
  for (const char* id : ids) {
    result.emplace_back(id);
  }
  return result;
}

const ProposedComponentTransform& flexible_subassembly_proposal_for(
    const AssemblyFlexibleSubassemblySolveResult& result, const char* component) {
  const auto found =
      std::find_if(result.local_result.proposed_transforms.begin(),
                   result.local_result.proposed_transforms.end(),
                   [component](const ProposedComponentTransform& proposal) {
                     return proposal.component_instance.value() == component;
                   });
  REQUIRE(found != result.local_result.proposed_transforms.end());
  return *found;
}

} // namespace

TEST_CASE("Flexible subassembly solver solves and atomically applies one child-local group",
          "[geometry][assembly-flexible-subassembly]") {
  Project project = make_flexible_subassembly_project();
  const AssemblyDocument* source_child = project.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(source_child != nullptr);
  const RigidTransform source_transform =
      source_child->find_component_instance(ComponentInstanceId("component.b"))->transform();
  const RigidTransform boundary_transform =
      project.assembly()
          .find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))
          ->transform();

  const AssemblyFlexibleSubassemblySolver solver;
  const auto first = solver.solve(
      project, flexible_subassembly_path({"subassembly.left"}),
      flexible_subassembly_group({"component.a", "component.b"}));
  const auto second = solver.solve(
      project, flexible_subassembly_path({"subassembly.left"}),
      flexible_subassembly_group({"component.a", "component.b"}));
  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == second.value());
  REQUIRE(first.value().converged());
  CHECK(first.value().assembly_document.value() == "assembly.child");
  CHECK(first.value().occurrence_path == flexible_subassembly_path({"subassembly.left"}));
  CHECK(flexible_subassembly_proposal_for(first.value(), "component.b")
            .proposed_transform.translation_mm.z ==
        Approx(8.0).margin(kFlexibleSubassemblySolveTolerance));
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_component_instance(ComponentInstanceId("component.b"))
            ->transform() == source_transform);

  const AssemblyFlexibleSubassemblySolveResultApplier applier;
  const auto applied = applier.apply(project, first.value());
  REQUIRE(applied);
  CHECK(applied.value() == 1U);
  const ComponentInstance* moved = project.find_assembly_document(DocumentId("assembly.child"))
                                       ->find_component_instance(ComponentInstanceId("component.b"));
  REQUIRE(moved != nullptr);
  CHECK(moved->transform().translation_mm.x ==
        Approx(5.0).margin(kFlexibleSubassemblySolveTolerance));
  CHECK(moved->transform().translation_mm.y ==
        Approx(-3.0).margin(kFlexibleSubassemblySolveTolerance));
  CHECK(moved->transform().translation_mm.z ==
        Approx(8.0).margin(kFlexibleSubassemblySolveTolerance));
  CHECK(project.assembly()
            .find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))
            ->transform() == boundary_transform);

  const AssemblyLeafOccurrenceResolver resolver;
  const auto leaves = resolver.resolve(project);
  REQUIRE(leaves);
  const auto moved_leaf = std::find_if(
      leaves.value().begin(), leaves.value().end(), [](const AssemblyLeafOccurrenceDescriptor& leaf) {
        return leaf.component_instance.value() == "component.b";
      });
  REQUIRE(moved_leaf != leaves.value().end());
  REQUIRE(moved_leaf->transforms_inner_to_outer.size() == 2U);
  CHECK(moved_leaf->transforms_inner_to_outer.front() == moved->transform());
  CHECK(moved_leaf->transforms_inner_to_outer.back() == boundary_transform);
}

TEST_CASE("Repeated rigid occurrences share one solved flexible child-document pose",
          "[geometry][assembly-flexible-subassembly]") {
  Project project = make_flexible_subassembly_project(true);
  const AssemblyFlexibleSubassemblySolver solver;
  const auto solved = solver.solve(
      project, flexible_subassembly_path({"subassembly.left"}),
      flexible_subassembly_group({"component.a", "component.b"}));
  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  const AssemblyFlexibleSubassemblySolveResultApplier applier;
  REQUIRE(applier.apply(project, solved.value()));

  const AssemblyLeafOccurrenceResolver resolver;
  const auto leaves = resolver.resolve(project);
  REQUIRE(leaves);
  REQUIRE(leaves.value().size() == 4U);

  std::size_t moved_leaf_count = 0U;
  bool saw_left = false;
  bool saw_right = false;
  for (const auto& leaf : leaves.value()) {
    if (leaf.component_instance.value() != "component.b") {
      continue;
    }
    ++moved_leaf_count;
    REQUIRE(leaf.transforms_inner_to_outer.size() == 2U);
    CHECK(leaf.transforms_inner_to_outer.front().translation_mm.z ==
          Approx(8.0).margin(kFlexibleSubassemblySolveTolerance));
    saw_left =
        saw_left || leaf.subassembly_path == flexible_subassembly_path({"subassembly.left"});
    saw_right =
        saw_right || leaf.subassembly_path == flexible_subassembly_path({"subassembly.right"});
  }
  CHECK(moved_leaf_count == 2U);
  CHECK(saw_left);
  CHECK(saw_right);
}

TEST_CASE("Flexible subassembly solver requires an exact active non-root occurrence path",
          "[geometry][assembly-flexible-subassembly]") {
  const AssemblyFlexibleSubassemblySolver solver;
  const auto components = flexible_subassembly_group({"component.a", "component.b"});

  Project project = make_flexible_subassembly_project();
  CHECK(solver.solve(project, {}, components).has_error());
  CHECK(solver
            .solve(project, flexible_subassembly_path({"subassembly.missing"}), components)
            .has_error());

  Project suppressed = make_flexible_subassembly_project(
      false, ComponentSuppressionState::Suppressed);
  CHECK(solver
            .solve(suppressed, flexible_subassembly_path({"subassembly.left"}), components)
            .has_error());
}

TEST_CASE("Flexible subassembly solve application rejects stale child component input atomically",
          "[geometry][assembly-flexible-subassembly]") {
  Project project = make_flexible_subassembly_project();
  const AssemblyFlexibleSubassemblySolver solver;
  const auto solved = solver.solve(
      project, flexible_subassembly_path({"subassembly.left"}),
      flexible_subassembly_group({"component.a", "component.b"}));
  REQUIRE(solved);
  REQUIRE(solved.value().converged());

  AssemblyDocument* child = project.find_assembly_document(DocumentId("assembly.child"));
  REQUIRE(child != nullptr);
  const RigidTransform changed{Vector3{1.0, 2.0, 30.0}, Vector3{}};
  REQUIRE(child->set_component_instance_transform(ComponentInstanceId("component.b"), changed));

  const AssemblyFlexibleSubassemblySolveResultApplier applier;
  CHECK(applier.apply(project, solved.value()).has_error());
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_component_instance(ComponentInstanceId("component.b"))
            ->transform() == changed);
}
