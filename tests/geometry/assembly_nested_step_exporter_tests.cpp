#include "blcad/geometry/assembly_step_exporter.hpp"

#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

struct StepBounds {
  double x_min = 0.0;
  double y_min = 0.0;
  double z_min = 0.0;
  double x_max = 0.0;
  double y_max = 0.0;
  double z_max = 0.0;
};

struct StepShapeSummary {
  StepBounds bounds;
  std::size_t solid_count = 0U;
  double volume_mm3 = 0.0;
};

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_plate_part(const char* id = "part.plate") {
  auto part = PartDocument::create(DocumentId(id), id);
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.width", "width", 10.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.height", "height", 20.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.thickness", "thickness", 2.0)));

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

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base_extrude"), "BaseExtrude", SketchId("sketch.base"),
      ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(part.value().add_feature(feature.value()));
  return part.value();
}

AssemblyDocument make_assembly(const char* id) {
  auto assembly = AssemblyDocument::create(DocumentId(id), id);
  REQUIRE(assembly);
  return assembly.value();
}

SubassemblyInstance make_subassembly(
    const char* id, const char* child, RigidTransform transform = identity_rigid_transform(),
    ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto instance = SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId(child),
                                              visibility, suppression, transform);
  REQUIRE(instance);
  return instance.value();
}

ComponentInstance make_component(
    const char* id, const char* part, RigidTransform transform = identity_rigid_transform(),
    ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto component = ComponentInstance::create(ComponentInstanceId(id), id, DocumentId(part),
                                             visibility, suppression,
                                             ComponentGroundingState::Free, transform);
  REQUIRE(component);
  return component.value();
}

StepShapeSummary read_step_shape_summary(const std::filesystem::path& path) {
  STEPControl_Reader reader;
  REQUIRE(reader.ReadFile(path.string().c_str()) == IFSelect_RetDone);
  REQUIRE(reader.TransferRoots() > 0);
  const TopoDS_Shape shape = reader.OneShape();
  REQUIRE_FALSE(shape.IsNull());

  Bnd_Box bounds;
  BRepBndLib::Add(shape, bounds);
  REQUIRE_FALSE(bounds.IsVoid());

  StepShapeSummary summary;
  bounds.Get(summary.bounds.x_min, summary.bounds.y_min, summary.bounds.z_min,
             summary.bounds.x_max, summary.bounds.y_max, summary.bounds.z_max);
  for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
    ++summary.solid_count;
  }
  GProp_GProps volume_properties;
  BRepGProp::VolumeProperties(shape, volume_properties);
  summary.volume_mm3 = volume_properties.Mass();
  return summary;
}

void check_equivalent_geometry(const StepShapeSummary& actual,
                               const StepShapeSummary& expected) {
  CHECK(actual.solid_count == expected.solid_count);
  CHECK(actual.volume_mm3 == Approx(expected.volume_mm3).margin(1.0e-6));
  CHECK(actual.bounds.x_min == Approx(expected.bounds.x_min).margin(1.0e-5));
  CHECK(actual.bounds.x_max == Approx(expected.bounds.x_max).margin(1.0e-5));
  CHECK(actual.bounds.y_min == Approx(expected.bounds.y_min).margin(1.0e-5));
  CHECK(actual.bounds.y_max == Approx(expected.bounds.y_max).margin(1.0e-5));
  CHECK(actual.bounds.z_min == Approx(expected.bounds.z_min).margin(1.0e-5));
  CHECK(actual.bounds.z_max == Approx(expected.bounds.z_max).margin(1.0e-5));
}

Project make_two_level_nested_project() {
  AssemblyDocument root = make_assembly("assembly.root");
  REQUIRE(root.add_subassembly_instance(make_subassembly(
      "subassembly.a", "assembly.child",
      RigidTransform{Vector3{100.0, 0.0, 0.0}, Vector3{0.0, 0.0, 90.0}})));
  REQUIRE(root.add_subassembly_instance(make_subassembly(
      "subassembly.z", "assembly.child",
      RigidTransform{Vector3{200.0, 0.0, 0.0}, Vector3{}})));

  AssemblyDocument child = make_assembly("assembly.child");
  REQUIRE(child.add_subassembly_instance(make_subassembly(
      "subassembly.inner", "assembly.grandchild",
      RigidTransform{Vector3{0.0, 20.0, 0.0}, Vector3{0.0, 0.0, 90.0}})));

  AssemblyDocument grandchild = make_assembly("assembly.grandchild");
  REQUIRE(grandchild.add_member_part(DocumentId("part.plate")));
  REQUIRE(grandchild.add_component_instance(make_component(
      "component.plate", "part.plate",
      RigidTransform{Vector3{10.0, 0.0, 0.0}, Vector3{}})));

  auto project = Project::create(DocumentId("project.nested"), "Nested", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(grandchild));
  REQUIRE(project.value().add_child_assembly_document(child));
  REQUIRE(project.value().add_part_document(make_plate_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

} // namespace

TEST_CASE("AssemblyStepExporter flattens repeated two-level rigid subassemblies",
          "[geometry][assembly-nested-step-export]") {
  const Project project = make_two_level_nested_project();
  const AssemblyStepExporter exporter;

  const auto built = exporter.build_posed_shape(project);
  REQUIRE(built);
  const ShapeSummary shape_summary = RectangleExtrusionAdapter{}.summarize(built.value());
  CHECK(shape_summary.solid_count == 2U);
  CHECK(shape_summary.volume_mm3 == Approx(800.0).margin(1.0e-6));

  const std::filesystem::path first_path =
      std::filesystem::temp_directory_path() / "blcad_nested_assembly_first.step";
  const std::filesystem::path second_path =
      std::filesystem::temp_directory_path() / "blcad_nested_assembly_second.step";
  std::filesystem::remove(first_path);
  std::filesystem::remove(second_path);

  const auto first = exporter.write_step(project, first_path.string());
  const auto second = exporter.write_step(project, second_path.string());
  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value().recomputed_part_count == 1U);
  CHECK(first.value().exported_component_count == 2U);
  CHECK(first.value().written_bytes > 0U);

  const StepShapeSummary first_summary = read_step_shape_summary(first_path);
  CHECK(first_summary.solid_count == 2U);
  CHECK(first_summary.volume_mm3 == Approx(800.0).margin(1.0e-6));
  CHECK(first_summary.bounds.x_min == Approx(65.0).margin(1.0e-5));
  CHECK(first_summary.bounds.x_max == Approx(210.0).margin(1.0e-5));
  CHECK(first_summary.bounds.y_min == Approx(-10.0).margin(1.0e-5));
  CHECK(first_summary.bounds.y_max == Approx(35.0).margin(1.0e-5));
  CHECK(first_summary.bounds.z_min == Approx(0.0).margin(1.0e-5));
  CHECK(first_summary.bounds.z_max == Approx(2.0).margin(1.0e-5));

  check_equivalent_geometry(read_step_shape_summary(second_path), first_summary);
  std::filesystem::remove(first_path);
  std::filesystem::remove(second_path);
}

TEST_CASE("Nested posed export excludes hidden and suppressed complete subtrees and child leaves",
          "[geometry][assembly-nested-step-export]") {
  AssemblyDocument root = make_assembly("assembly.root");
  REQUIRE(root.add_subassembly_instance(
      make_subassembly("subassembly.visible", "assembly.child")));
  REQUIRE(root.add_subassembly_instance(make_subassembly(
      "subassembly.hidden", "assembly.child",
      RigidTransform{Vector3{100.0, 0.0, 0.0}, Vector3{}}, ComponentVisibility::Hidden)));
  REQUIRE(root.add_subassembly_instance(make_subassembly(
      "subassembly.suppressed", "assembly.child",
      RigidTransform{Vector3{200.0, 0.0, 0.0}, Vector3{}}, ComponentVisibility::Visible,
      ComponentSuppressionState::Suppressed)));

  AssemblyDocument child = make_assembly("assembly.child");
  REQUIRE(child.add_member_part(DocumentId("part.plate")));
  REQUIRE(child.add_component_instance(make_component("component.visible", "part.plate")));
  REQUIRE(child.add_component_instance(make_component(
      "component.hidden", "part.plate",
      RigidTransform{Vector3{300.0, 0.0, 0.0}, Vector3{}}, ComponentVisibility::Hidden)));

  auto project = Project::create(DocumentId("project.filtered"), "Filtered", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child));
  REQUIRE(project.value().add_part_document(make_plate_part()));

  const auto built = AssemblyStepExporter{}.build_posed_shape(project.value());
  REQUIRE(built);
  const ShapeSummary summary = RectangleExtrusionAdapter{}.summarize(built.value());
  CHECK(summary.solid_count == 1U);
  CHECK(summary.volume_mm3 == Approx(400.0).margin(1.0e-6));
}

TEST_CASE("Nested posed export fails closed when hierarchy leaves are all filtered",
          "[geometry][assembly-nested-step-export]") {
  AssemblyDocument root = make_assembly("assembly.root");
  REQUIRE(root.add_subassembly_instance(make_subassembly(
      "subassembly.hidden", "assembly.child", identity_rigid_transform(),
      ComponentVisibility::Hidden)));
  AssemblyDocument child = make_assembly("assembly.child");
  REQUIRE(child.add_member_part(DocumentId("part.plate")));
  REQUIRE(child.add_component_instance(make_component("component.plate", "part.plate")));

  auto project = Project::create(DocumentId("project.empty"), "Empty", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child));
  REQUIRE(project.value().add_part_document(make_plate_part()));

  const auto built = AssemblyStepExporter{}.build_posed_shape(project.value());
  REQUIRE(built.has_error());
  CHECK(built.error().category() == ErrorCategory::Export);
  CHECK(built.error().message() ==
        "posed assembly export requires at least one visible active component");
}

TEST_CASE("Nested posed export fails closed on an unresolved child leaf part",
          "[geometry][assembly-nested-step-export]") {
  AssemblyDocument root = make_assembly("assembly.root");
  REQUIRE(root.add_subassembly_instance(
      make_subassembly("subassembly.child", "assembly.child")));
  AssemblyDocument child = make_assembly("assembly.child");
  REQUIRE(child.add_member_part(DocumentId("part.missing")));
  REQUIRE(child.add_component_instance(make_component("component.missing", "part.missing")));

  auto project = Project::create(DocumentId("project.missing"), "Missing", root);
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child));

  const auto built = AssemblyStepExporter{}.build_posed_shape(project.value());
  REQUIRE(built.has_error());
  CHECK(built.error().category() == ErrorCategory::Validation);
  CHECK(built.error().object_id() == "part.missing");
  CHECK(built.error().message() ==
        "assembly member part must resolve to an owned project part document");
}
