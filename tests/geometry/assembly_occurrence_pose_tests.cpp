#include "../support/geometry_volume_oracle.hpp"
#include "assembly_cross_hierarchy_test_support.hpp"
#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_contact_analyzer.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"
#include "blcad/geometry/assembly_occurrence_solver.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/body_result_inspector.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <STEPControl_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <numbers>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;
namespace {
PartDocument cylinder() {
  using blcad::geometry::test_support::length_parameter;
  auto part = PartDocument::create(DocumentId("cylinder"), "Cylinder");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(length_parameter("diameter", "Diameter", 20)));
  REQUIRE(part.value().add_parameter(length_parameter("length", "Length", 10)));
  REQUIRE(part.value().add_parameter(length_parameter("bore", "Bore", 6)));
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));
  for (const char* id : {"base", "hole"}) {
    auto sketch = Sketch::create(SketchId(std::string(id) + ".sketch"), id, xy.value().id());
    REQUIRE(sketch);
    auto circle = CircleProfile::create(
        ProfileId(id), ParameterId(std::string(id) == "base" ? "diameter" : "bore"));
    REQUIRE(circle);
    REQUIRE(sketch.value().add_profile(circle.value()));
    REQUIRE(part.value().add_sketch(sketch.value()));
  }
  auto base = Feature::create_additive_extrude(FeatureId("base"), "Base", SketchId("base.sketch"),
                                               ParameterId("length"));
  REQUIRE(base);
  REQUIRE(part.value().add_feature(base.value()));
  auto hole = Feature::create_subtractive_extrude(
      FeatureId("hole"), "Hole", SketchId("hole.sketch"), FeatureId("base"),
      SubtractiveExtrudeDepth::ThroughAll, ExtrudeDirection::SketchNormal);
  REQUIRE(hole);
  REQUIRE(part.value().add_feature(hole.value()));
  return part.value();
}
Project cylinders() {
  auto root = AssemblyDocument::create(DocumentId("root"), "Root");
  REQUIRE(root);
  auto child = AssemblyDocument::create(DocumentId("child"), "Cylinder assembly");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("cylinder")));
  for (const char* id : {"sleeve", "rod"}) {
    auto component =
        ComponentInstance::create(ComponentInstanceId(id), id, DocumentId("cylinder"),
                                  ComponentVisibility::Visible, ComponentSuppressionState::Active,
                                  std::string(id) == "sleeve" ? ComponentGroundingState::Grounded
                                                              : ComponentGroundingState::Free);
    REQUIRE(component);
    REQUIRE(child.value().add_component_instance(component.value()));
  }
  auto fixed = AssemblyConstraintTarget::create(ComponentInstanceId("sleeve"), "hole.seat");
  REQUIRE(fixed);
  auto moving = AssemblyConstraintTarget::create(ComponentInstanceId("rod"), "hole.seat");
  REQUIRE(moving);
  auto zero = Quantity::linear_displacement_mm(0);
  REQUIRE(zero);
  auto max = Quantity::linear_displacement_mm(100);
  REQUIRE(max);
  auto slot = AssemblyJointCoordinateSlot::create(AssemblyJointCoordinateRole::Translation,
                                                  AssemblyJointCoordinateKind::Linear, zero.value(),
                                                  zero.value(), max.value());
  REQUIRE(slot);
  auto joint =
      AssemblyJoint::create(AssemblyJointId("slider"), "Slider", AssemblyJointType::Prismatic,
                            fixed.value(), moving.value(), AssemblyJointState::Active,
                            std::vector<AssemblyJointCoordinateSlot>{slot.value()});
  REQUIRE(joint);
  REQUIRE(child.value().add_joint(joint.value()));
  for (const char* id : {"left", "right"}) {
    auto instance = SubassemblyInstance::create(
        SubassemblyInstanceId(id), id, child.value().id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active,
        std::string(id) == "left" ? RigidTransform{{100, 0, 0}, {}}
                                  : RigidTransform{{-100, 0, 0}, {0, 90, 0}});
    REQUIRE(instance);
    REQUIRE(root.value().add_subassembly_instance(instance.value()));
  }
  auto project = Project::create(DocumentId("project"), "Independent cylinders", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(cylinder()));
  for (const char* id : {"left", "right"}) {
    auto pose = AssemblyOccurrencePose::create({SubassemblyInstanceId(id)}, DocumentId("child"),
                                               AssemblyOccurrenceRigidity::Flexible);
    REQUIRE(pose);
    REQUIRE(project.value().set_occurrence_pose(pose.value()));
  }
  return project.value();
}
void extend(Project& project, const char* occurrence, double value) {
  auto distance = Quantity::linear_displacement_mm(value);
  REQUIRE(distance);
  AssemblyOccurrenceSolver solver;
  auto result = solver.move(
      project, {SubassemblyInstanceId(occurrence)},
      {AssemblyJointId("slider"), {{AssemblyJointCoordinateRole::Translation, distance.value()}}});
  REQUIRE(result);
  REQUIRE(result.value().converged());
  REQUIRE(solver.apply(project, result.value()));
}
} // namespace

TEST_CASE("Independent cylinder motion preserves the shared child and exact local pose",
          "[parity-b193][occurrence-pose][occurrence-motion]") {
  auto project = cylinders();
  auto child =
      serialize_assembly_document_to_json(*project.find_assembly_document(DocumentId("child")));
  REQUIRE(child);
  extend(project, "left", 10);
  extend(project, "right", 30);
  CHECK(project
            .effective_component_transform({SubassemblyInstanceId("left")},
                                           ComponentInstanceId("rod"))
            .value()
            .translation_mm.z == Approx(10).margin(1e-6));
  CHECK(project
            .effective_component_transform({SubassemblyInstanceId("right")},
                                           ComponentInstanceId("rod"))
            .value()
            .translation_mm.z == Approx(30).margin(1e-6));
  CHECK(serialize_assembly_document_to_json(*project.find_assembly_document(DocumentId("child")))
            .value() == child.value());
  auto saved = serialize_project_to_json(project);
  REQUIRE(saved);
  auto reopened = deserialize_project_from_json(saved.value());
  REQUIRE(reopened);
  CHECK(serialize_project_to_json(reopened.value()).value() == saved.value());
  extend(reopened.value(), "left", 20);
  CHECK(reopened.value()
            .effective_component_transform({SubassemblyInstanceId("right")},
                                           ComponentInstanceId("rod"))
            .value()
            .translation_mm.z == Approx(30).margin(1e-6));
  auto distance = Quantity::linear_displacement_mm(25);
  REQUIRE(distance);
  AssemblyOccurrenceSolver solver;
  auto pending = solver.move(
      project, {SubassemblyInstanceId("left")},
      {AssemblyJointId("slider"), {{AssemblyJointCoordinateRole::Translation, distance.value()}}});
  REQUIRE(pending);
  extend(project, "right", 35);
  auto before = serialize_project_to_json(project);
  REQUIRE(before);
  CHECK_FALSE(solver.apply(project, pending.value()));
  CHECK(serialize_project_to_json(project).value() == before.value());
}

TEST_CASE("Posed geometry contact physical properties and both STEP paths share cylinder poses",
          "[parity-b193][occurrence-pose][occurrence-consumers]") {
  auto project = cylinders();
  extend(project, "left", 10);
  extend(project, "right", 30);
  auto shape = AssemblyStepExporter{}.build_posed_shape(project);
  REQUIRE(shape);
  CHECK(blcad::test::classify_solid_point(shape.value(), {108, 0, 15}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(shape.value(), {-65, 8, 0}) ==
        blcad::test::SolidPointLocation::Inside);
  CHECK(blcad::test::classify_solid_point(shape.value(), {-80, 8, 0}) ==
        blcad::test::SolidPointLocation::Outside);
  const double volume = 4 * std::numbers::pi * 91 * 10;
  auto properties = BodyResultInspector{}.inspect_physical_properties(project);
  REQUIRE(properties);
  CHECK(properties.value().data().volume_mm3 == Approx(volume).epsilon(1e-8));
  REQUIRE(properties.value().data().volume_centroid_mm);
  CHECK(properties.value().data().volume_centroid_mm->x == Approx(10).margin(1e-6));
  CHECK(properties.value().data().volume_centroid_mm->z == Approx(5).margin(1e-6));
  auto contact = AssemblyContactAnalyzer{}.analyze(project);
  REQUIRE(contact);
  CHECK(contact.value().component_occurrence_count == 4);
  CHECK(contact.value().evaluated_pair_count == 6);
  CHECK(std::count_if(contact.value().records.begin(), contact.value().records.end(),
                      [](const auto& item) {
                        return item.classification == AssemblyContactClassification::Touching;
                      }) == 1);
  const auto dir = std::filesystem::temp_directory_path() / "blcad-occurrence-pose-proof";
  std::filesystem::create_directories(dir);
  REQUIRE(AssemblyStepExporter{}.write_step(project, (dir / "flat.step").string()));
  REQUIRE(AssemblyStructuredStepExporter{}.write_step(project, (dir / "structured.step").string()));
  for (const char* file : {"flat.step", "structured.step"}) {
    STEPControl_Reader reader;
    REQUIRE(reader.ReadFile((dir / file).c_str()) == IFSelect_RetDone);
    REQUIRE(reader.TransferRoots() > 0);
    auto imported = reader.OneShape();
    std::size_t solids = 0;
    for (TopExp_Explorer it(imported, TopAbs_SOLID); it.More(); it.Next())
      ++solids;
    CHECK(solids == 4);
    GProp_GProps mass;
    BRepGProp::VolumeProperties(imported, mass);
    CHECK(mass.Mass() == Approx(volume).epsilon(1e-7));
    CHECK(mass.CentreOfMass().X() == Approx(10).margin(1e-6));
    CHECK(mass.CentreOfMass().Z() == Approx(5).margin(1e-6));
  }
  auto hidden = project;
  REQUIRE(hidden.find_assembly_document(DocumentId("child"))
              ->set_component_instance_visibility(ComponentInstanceId("rod"),
                                                  ComponentVisibility::Hidden));
  auto hidden_physical = BodyResultInspector{}.inspect_physical_properties(hidden);
  REQUIRE(hidden_physical);
  CHECK(hidden_physical.value().data().volume_mm3 == Approx(volume).epsilon(1e-8));
  auto hidden_contact = AssemblyContactAnalyzer{}.analyze(hidden);
  REQUIRE(hidden_contact);
  CHECK(hidden_contact.value().component_occurrence_count == 2);
}

TEST_CASE("Cross hierarchy solving assigns separate authorities to repeated flexible paths",
          "[parity-b193][occurrence-pose][occurrence-cross-solve]") {
  using namespace blcad::geometry::test_support;
  auto project = distance_project(20.0, true);
  auto relation = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.independent"), "Independent distance",
      AssemblyConstraintType::Distance,
      endpoint({"subassembly.left"}, "component.child", "feature.base_extrude.top"),
      endpoint({"subassembly.right"}, "component.child", "feature.base_extrude.top"),
      AssemblyConstraintState::Active, distance(30, "constraint.independent"));
  REQUIRE(relation);
  REQUIRE(project.add_cross_hierarchy_constraint(relation.value()));
  for (const char* id : {"subassembly.left", "subassembly.right"}) {
    auto pose =
        AssemblyOccurrencePose::create({SubassemblyInstanceId(id)}, DocumentId("assembly.child"),
                                       AssemblyOccurrenceRigidity::Flexible);
    REQUIRE(pose);
    REQUIRE(project.set_occurrence_pose(pose.value()));
  }
  const auto child = serialize_assembly_document_to_json(
      *project.find_assembly_document(DocumentId("assembly.child")));
  REQUIRE(child);
  const auto group = only_group(project);
  CHECK(group.authorities.size() == 3);
  auto result = AssemblyCrossHierarchyConstraintSolver{}.solve(project, group);
  REQUIRE(result);
  REQUIRE(result.value().converged());
  REQUIRE(AssemblyCrossHierarchySolveResultApplier{}.apply(project, result.value()));
  CHECK(serialize_assembly_document_to_json(
            *project.find_assembly_document(DocumentId("assembly.child")))
            .value() == child.value());
  auto left = project.effective_component_transform({SubassemblyInstanceId("subassembly.left")},
                                                    ComponentInstanceId("component.child"));
  auto right = project.effective_component_transform({SubassemblyInstanceId("subassembly.right")},
                                                     ComponentInstanceId("component.child"));
  REQUIRE(left);
  REQUIRE(right);
  CHECK(left.value() != right.value());
}
