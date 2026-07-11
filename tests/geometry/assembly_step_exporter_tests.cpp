#include "blcad/geometry/assembly_step_exporter.hpp"

#include "blcad/geometry/rectangle_extrusion_adapter.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <TopoDS_Shape.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

struct ComponentSpec {
  const char* id;
  ComponentVisibility visibility = ComponentVisibility::Visible;
  ComponentSuppressionState suppression = ComponentSuppressionState::Active;
  RigidTransform transform = identity_rigid_transform();
};

struct StepBounds {
  double x_min = 0.0;
  double y_min = 0.0;
  double z_min = 0.0;
  double x_max = 0.0;
  double y_max = 0.0;
  double z_max = 0.0;
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

Project make_project(std::initializer_list<ComponentSpec> components) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.posed_export"), "PosedExport");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.plate")));
  for (const auto& spec : components) {
    auto component = ComponentInstance::create(
        ComponentInstanceId(spec.id), spec.id, DocumentId("part.plate"), spec.visibility,
        spec.suppression, ComponentGroundingState::Free, spec.transform);
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }

  auto project = Project::create(DocumentId("project.posed_export"), "PosedExportProject",
                                 assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_plate_part()));
  return project.value();
}

StepBounds read_step_bounds(const std::filesystem::path& path) {
  STEPControl_Reader reader;
  REQUIRE(reader.ReadFile(path.string().c_str()) == IFSelect_RetDone);
  REQUIRE(reader.TransferRoots() > 0);
  const TopoDS_Shape shape = reader.OneShape();
  REQUIRE_FALSE(shape.IsNull());

  Bnd_Box bounds;
  BRepBndLib::Add(shape, bounds);
  REQUIRE_FALSE(bounds.IsVoid());

  StepBounds result;
  bounds.Get(result.x_min, result.y_min, result.z_min, result.x_max, result.y_max, result.z_max);
  return result;
}

std::string read_step_data_section(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.good());
  const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  const std::size_t data_start = content.find("DATA;");
  REQUIRE(data_start != std::string::npos);
  return content.substr(data_start);
}

} // namespace

TEST_CASE("AssemblyStepExporter poses repeated part instances and writes deterministic STEP data",
          "[geometry][assembly-step-export]") {
  Project project = make_project(
      {{"component.a"},
       {"component.b", ComponentVisibility::Visible, ComponentSuppressionState::Active,
        RigidTransform{Vector3{30.0, 0.0, 0.0}, Vector3{0.0, 0.0, 90.0}}}});

  const AssemblyStepExporter exporter;
  const auto built = exporter.build_posed_shape(project);
  REQUIRE(built);
  const ShapeSummary shape_summary = RectangleExtrusionAdapter{}.summarize(built.value());
  CHECK(shape_summary.solid_count == 2U);
  CHECK(shape_summary.volume_mm3 == Approx(800.0).margin(1.0e-6));

  const std::filesystem::path first_path =
      std::filesystem::temp_directory_path() / "blcad_posed_assembly_first.step";
  const std::filesystem::path second_path =
      std::filesystem::temp_directory_path() / "blcad_posed_assembly_second.step";
  std::filesystem::remove(first_path);
  std::filesystem::remove(second_path);

  const auto first = exporter.write_step(project, first_path.string());
  const auto second = exporter.write_step(project, second_path.string());
  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value().recomputed_part_count == 1U);
  CHECK(first.value().exported_component_count == 2U);
  CHECK(first.value().written_bytes > 0U);

  const StepBounds bounds = read_step_bounds(first_path);
  CHECK(bounds.x_min == Approx(-5.0).margin(1.0e-5));
  CHECK(bounds.x_max == Approx(40.0).margin(1.0e-5));
  CHECK(bounds.y_min == Approx(-10.0).margin(1.0e-5));
  CHECK(bounds.y_max == Approx(10.0).margin(1.0e-5));
  CHECK(bounds.z_min == Approx(0.0).margin(1.0e-5));
  CHECK(bounds.z_max == Approx(2.0).margin(1.0e-5));
  CHECK(read_step_data_section(first_path) == read_step_data_section(second_path));

  std::filesystem::remove(first_path);
  std::filesystem::remove(second_path);
}

TEST_CASE("AssemblyStepExporter excludes hidden and suppressed component occurrences",
          "[geometry][assembly-step-export]") {
  Project project = make_project(
      {{"component.visible"},
       {"component.hidden", ComponentVisibility::Hidden, ComponentSuppressionState::Active,
        RigidTransform{Vector3{50.0, 0.0, 0.0}, Vector3{}}},
       {"component.suppressed", ComponentVisibility::Visible,
        ComponentSuppressionState::Suppressed,
        RigidTransform{Vector3{100.0, 0.0, 0.0}, Vector3{}}}});

  const AssemblyStepExporter exporter;
  const auto built = exporter.build_posed_shape(project);
  REQUIRE(built);
  const ShapeSummary shape_summary = RectangleExtrusionAdapter{}.summarize(built.value());
  CHECK(shape_summary.solid_count == 1U);
  CHECK(shape_summary.volume_mm3 == Approx(400.0).margin(1.0e-6));

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "blcad_posed_assembly_filtered.step";
  std::filesystem::remove(path);
  const auto written = exporter.write_step(project, path.string());
  REQUIRE(written);
  CHECK(written.value().recomputed_part_count == 1U);
  CHECK(written.value().exported_component_count == 1U);

  const StepBounds bounds = read_step_bounds(path);
  CHECK(bounds.x_min == Approx(-5.0).margin(1.0e-5));
  CHECK(bounds.x_max == Approx(5.0).margin(1.0e-5));
  CHECK(bounds.y_min == Approx(-10.0).margin(1.0e-5));
  CHECK(bounds.y_max == Approx(10.0).margin(1.0e-5));

  std::filesystem::remove(path);
}

TEST_CASE("AssemblyStepExporter fails closed on an unresolvable project member",
          "[geometry][assembly-step-export]") {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.missing_part"), "MissingPart");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.missing")));
  auto component = ComponentInstance::create(ComponentInstanceId("component.missing"), "Missing",
                                             DocumentId("part.missing"));
  REQUIRE(component);
  REQUIRE(assembly.value().add_component_instance(component.value()));
  auto project = Project::create(DocumentId("project.missing_part"), "MissingPartProject",
                                 assembly.value());
  REQUIRE(project);

  const auto built = AssemblyStepExporter{}.build_posed_shape(project.value());
  REQUIRE(built.has_error());
  CHECK(built.error().category() == ErrorCategory::Validation);
  CHECK(built.error().object_id() == "part.missing");
  CHECK(built.error().message() ==
        "assembly member part must resolve to an owned project part document");
}

TEST_CASE("AssemblyStepExporter fails when a referenced part recomputes without a final shape",
          "[geometry][assembly-step-export]") {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.empty_part"), "EmptyPart");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.empty")));
  auto component = ComponentInstance::create(ComponentInstanceId("component.empty"), "Empty",
                                             DocumentId("part.empty"));
  REQUIRE(component);
  REQUIRE(assembly.value().add_component_instance(component.value()));
  auto project = Project::create(DocumentId("project.empty_part"), "EmptyPartProject",
                                 assembly.value());
  REQUIRE(project);
  auto empty_part = PartDocument::create(DocumentId("part.empty"), "EmptyPart");
  REQUIRE(empty_part);
  REQUIRE(project.value().add_part_document(empty_part.value()));

  const auto built = AssemblyStepExporter{}.build_posed_shape(project.value());
  REQUIRE(built.has_error());
  CHECK(built.error().category() == ErrorCategory::Geometry);
  CHECK(built.error().object_id() == "part.empty");
  CHECK(built.error().message() == "assembly export part recompute produced no final shape");
}
