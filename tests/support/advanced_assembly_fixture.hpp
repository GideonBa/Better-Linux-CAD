#pragma once
#include "blcad/core/project.hpp"
#include "engineering_resource_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
namespace blcad::test::advanced_assembly {
inline Parameter length_parameter(const char* id, const char* name, double value) {
  auto q = Quantity::length_mm(value);
  REQUIRE(q);
  auto p = Parameter::create_length(ParameterId(id), name, q.value());
  REQUIRE(p);
  return p.value();
}
inline PartDocument cylinder() {
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
  engineering_fixture::annotate(part.value());
  return part.value();
}
inline Project cylinders() {
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
  return project.value();
}
} // namespace blcad::test::advanced_assembly
