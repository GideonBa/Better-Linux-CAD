#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_step_exporter.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"

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
#include <fstream>
#include <iterator>
#include <string>

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

Parameter length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument plate_part() {
  auto part = PartDocument::create(DocumentId("part.plate"), "Plate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(length_parameter("part.width", "width", 10.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.height", "height", 20.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.thickness", "thickness", 2.0)));

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

ComponentInstance component(
    const char* id, ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active,
    RigidTransform transform = identity_rigid_transform()) {
  auto value = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.plate"), visibility, suppression,
      ComponentGroundingState::Free, transform);
  REQUIRE(value);
  return value.value();
}

SubassemblyInstance occurrence(
    const char* id, const char* child_document,
    ComponentVisibility visibility = ComponentVisibility::Visible,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active,
    RigidTransform transform = identity_rigid_transform()) {
  auto value = SubassemblyInstance::create(SubassemblyInstanceId(id), id,
                                           DocumentId(child_document), visibility, suppression,
                                           transform);
  REQUIRE(value);
  return value.value();
}

Project repeated_root_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.plate")));
  REQUIRE(root.value().add_component_instance(component("component.b", ComponentVisibility::Visible,
                                                        ComponentSuppressionState::Active,
                                                        RigidTransform{Vector3{30.0, 0.0, 0.0},
                                                                       Vector3{0.0, 0.0, 90.0}})));
  REQUIRE(root.value().add_component_instance(component("component.a")));

  auto project = Project::create(DocumentId("project.structured.root"), "StructuredRoot",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(plate_part()));
  return project.value();
}

Project repeated_nested_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.right", "assembly.child", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{100.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0}})));
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.left", "assembly.child", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{-100.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0}})));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.plate")));
  REQUIRE(child.value().add_component_instance(component(
      "component.child", ComponentVisibility::Visible, ComponentSuppressionState::Active,
      RigidTransform{Vector3{4.0, -6.0, 0.0}, Vector3{0.0, 0.0, 10.0}})));
  REQUIRE(child.value().add_subassembly_instance(occurrence(
      "subassembly.inner", "assembly.grandchild", ComponentVisibility::Visible,
      ComponentSuppressionState::Active,
      RigidTransform{Vector3{0.0, 30.0, 0.0}, Vector3{0.0, 20.0, 0.0}})));

  auto grandchild = AssemblyDocument::create(DocumentId("assembly.grandchild"), "Grandchild");
  REQUIRE(grandchild);
  REQUIRE(grandchild.value().add_member_part(DocumentId("part.plate")));
  REQUIRE(grandchild.value().add_component_instance(component(
      "component.grand", ComponentVisibility::Visible, ComponentSuppressionState::Active,
      RigidTransform{Vector3{0.0, 0.0, 5.0}, Vector3{30.0, 0.0, 0.0}})));

  auto project = Project::create(DocumentId("project.structured.nested"), "StructuredNested",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_child_assembly_document(grandchild.value()));
  REQUIRE(project.value().add_part_document(plate_part()));
  return project.value();
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

std::string read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  REQUIRE(stream.good());
  return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::size_t count_text(const std::string& content, const std::string& needle) {
  std::size_t count = 0U;
  std::size_t position = 0U;
  while ((position = content.find(needle, position)) != std::string::npos) {
    ++count;
    position += needle.size();
  }
  return count;
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

} // namespace

TEST_CASE("Structured STEP reuses one part definition for repeated root components",
          "[geometry][assembly-structured-step-export]") {
  Project project = repeated_root_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);

  const std::filesystem::path structured_path =
      std::filesystem::temp_directory_path() / "blcad_structured_root.step";
  const std::filesystem::path flattened_path =
      std::filesystem::temp_directory_path() / "blcad_structured_root_flattened.step";
  std::filesystem::remove(structured_path);
  std::filesystem::remove(flattened_path);

  const auto written = AssemblyStructuredStepExporter{}.write_step(project, structured_path.string());
  REQUIRE(written);
  CHECK(written.value().recomputed_part_count == 1U);
  CHECK(written.value().exported_assembly_occurrence_count == 1U);
  CHECK(written.value().exported_component_occurrence_count == 2U);
  CHECK(written.value().part_product_definition_count == 1U);
  CHECK(written.value().written_bytes > 0U);

  const std::string content = read_file(structured_path);
  CHECK(content.find("blcad:assembly-occurrence:root") != std::string::npos);
  CHECK(content.find("blcad:part-definition:part.plate") != std::string::npos);
  CHECK(content.find("blcad:component-occurrence:root/component.a") != std::string::npos);
  CHECK(content.find("blcad:component-occurrence:root/component.b") != std::string::npos);
  CHECK(count_text(content, "NEXT_ASSEMBLY_USAGE_OCCURRENCE") >= 2U);

  const auto flattened = AssemblyStepExporter{}.write_step(project, flattened_path.string());
  REQUIRE(flattened);
  check_equivalent_geometry(read_step_shape_summary(structured_path),
                            read_step_shape_summary(flattened_path));

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());

  std::filesystem::remove(structured_path);
  std::filesystem::remove(flattened_path);
}

TEST_CASE("Structured STEP preserves repeated child and nested assembly occurrence products",
          "[geometry][assembly-structured-step-export]") {
  Project project = repeated_nested_project();
  const std::filesystem::path structured_path =
      std::filesystem::temp_directory_path() / "blcad_structured_nested.step";
  const std::filesystem::path flattened_path =
      std::filesystem::temp_directory_path() / "blcad_structured_nested_flattened.step";
  std::filesystem::remove(structured_path);
  std::filesystem::remove(flattened_path);

  const auto written = AssemblyStructuredStepExporter{}.write_step(project, structured_path.string());
  REQUIRE(written);
  CHECK(written.value().recomputed_part_count == 1U);
  CHECK(written.value().exported_assembly_occurrence_count == 5U);
  CHECK(written.value().exported_component_occurrence_count == 4U);
  CHECK(written.value().part_product_definition_count == 1U);

  const std::string content = read_file(structured_path);
  CHECK(content.find("blcad:assembly-occurrence:subassembly.left") != std::string::npos);
  CHECK(content.find("blcad:assembly-occurrence:subassembly.right") != std::string::npos);
  CHECK(content.find("blcad:assembly-occurrence:subassembly.left/subassembly.inner") !=
        std::string::npos);
  CHECK(content.find("blcad:assembly-occurrence:subassembly.right/subassembly.inner") !=
        std::string::npos);
  CHECK(content.find("blcad:part-definition:part.plate") != std::string::npos);
  // STEPCAFControl_Writer only guarantees names/validation properties for top-level shapes.
  // Exact nested component occurrence names are therefore asserted at the exchange-graph layer;
  // the written STEP contract proves the complete nested usage graph plus posed geometry here.
  CHECK(count_text(content, "NEXT_ASSEMBLY_USAGE_OCCURRENCE") >= 8U);

  const auto flattened = AssemblyStepExporter{}.write_step(project, flattened_path.string());
  REQUIRE(flattened);
  check_equivalent_geometry(read_step_shape_summary(structured_path),
                            read_step_shape_summary(flattened_path));

  std::filesystem::remove(structured_path);
  std::filesystem::remove(flattened_path);
}

TEST_CASE("Structured STEP mirrors visible-active component and hierarchy filtering",
          "[geometry][assembly-structured-step-export]") {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.plate")));
  REQUIRE(root.value().add_component_instance(component("component.visible")));
  REQUIRE(root.value().add_component_instance(
      component("component.hidden", ComponentVisibility::Hidden)));
  REQUIRE(root.value().add_component_instance(component(
      "component.suppressed", ComponentVisibility::Visible,
      ComponentSuppressionState::Suppressed)));
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.hidden", "assembly.child", ComponentVisibility::Hidden)));
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.suppressed", "assembly.child", ComponentVisibility::Visible,
      ComponentSuppressionState::Suppressed)));
  REQUIRE(root.value().add_subassembly_instance(
      occurrence("subassembly.visible", "assembly.child")));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.plate")));
  REQUIRE(child.value().add_component_instance(component("component.child")));

  auto project = Project::create(DocumentId("project.structured.filter"), "StructuredFilter",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(plate_part()));

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "blcad_structured_filtered.step";
  std::filesystem::remove(path);
  const auto written = AssemblyStructuredStepExporter{}.write_step(project.value(), path.string());
  REQUIRE(written);
  CHECK(written.value().exported_assembly_occurrence_count == 2U);
  CHECK(written.value().exported_component_occurrence_count == 2U);

  const std::string content = read_file(path);
  CHECK(content.find("component.visible") != std::string::npos);
  CHECK(content.find("subassembly.visible") != std::string::npos);
  CHECK(content.find("component.hidden") == std::string::npos);
  CHECK(content.find("component.suppressed") == std::string::npos);
  CHECK(content.find("subassembly.hidden") == std::string::npos);
  CHECK(content.find("subassembly.suppressed") == std::string::npos);

  std::filesystem::remove(path);
}

TEST_CASE("Structured STEP fails closed without an output path or exported component",
          "[geometry][assembly-structured-step-export]") {
  Project project = repeated_root_project();
  CHECK(AssemblyStructuredStepExporter{}.write_step(project, "").has_error());

  auto root = AssemblyDocument::create(DocumentId("assembly.empty"), "Empty");
  REQUIRE(root);
  auto empty_project = Project::create(DocumentId("project.structured.empty"), "Empty",
                                       root.value());
  REQUIRE(empty_project);
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "blcad_structured_empty.step";
  std::filesystem::remove(path);
  CHECK(AssemblyStructuredStepExporter{}.write_step(empty_project.value(), path.string())
            .has_error());
  CHECK_FALSE(std::filesystem::exists(path));
}
