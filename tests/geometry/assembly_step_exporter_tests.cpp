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
#include <initializer_list>

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

  auto feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
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

  auto project =
      Project::create(DocumentId("project.posed_export"), "PosedExportProject", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_plate_part()));
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
  bounds.Get(summary.bounds.x_min, summary.bounds.y_min, summary.bounds.z_min, summary.bounds.x_max,
             summary.bounds.y_max, summary.bounds.z_max);

  for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
    ++summary.solid_count;
  }

  GProp_GProps volume_properties;
  BRepGProp::VolumeProperties(shape, volume_properties);
  summary.volume_mm3 = volume_properties.Mass();
  return summary;
}

void check_equivalent_step_geometry(const StepShapeSummary& actual,
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

TEST_CASE("AssemblyStepExporter poses repeated part instances and preserves repeat-export geometry",
          "[geometry][assembly-step-export]") {
  Project project =
      make_project({{"component.a"},
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

  const StepShapeSummary first_step = read_step_shape_summary(first_path);
  CHECK(first_step.solid_count == 2U);
  CHECK(first_step.volume_mm3 == Approx(800.0).margin(1.0e-6));
  CHECK(first_step.bounds.x_min == Approx(-5.0).margin(1.0e-5));
  CHECK(first_step.bounds.x_max == Approx(40.0).margin(1.0e-5));
  CHECK(first_step.bounds.y_min == Approx(-10.0).margin(1.0e-5));
  CHECK(first_step.bounds.y_max == Approx(10.0).margin(1.0e-5));
  CHECK(first_step.bounds.z_min == Approx(0.0).margin(1.0e-5));
  CHECK(first_step.bounds.z_max == Approx(2.0).margin(1.0e-5));

  const StepShapeSummary second_step = read_step_shape_summary(second_path);
  check_equivalent_step_geometry(second_step, first_step);

  std::filesystem::remove(first_path);
  std::filesystem::remove(second_path);
}

TEST_CASE("AssemblyStepExporter excludes hidden and suppressed component occurrences",
          "[geometry][assembly-step-export]") {
  Project project = make_project(
      {{"component.visible"},
       {"component.hidden", ComponentVisibility::Hidden, ComponentSuppressionState::Active,
        RigidTransform{Vector3{50.0, 0.0, 0.0}, Vector3{}}},
       {"component.suppressed", ComponentVisibility::Visible, ComponentSuppressionState::Suppressed,
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

  const StepBounds bounds = read_step_shape_summary(path).bounds;
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
  auto project =
      Project::create(DocumentId("project.missing_part"), "MissingPartProject", assembly.value());
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
  auto project =
      Project::create(DocumentId("project.empty_part"), "EmptyPartProject", assembly.value());
  REQUIRE(project);
  auto empty_part = PartDocument::create(DocumentId("part.empty"), "EmptyPart");
  REQUIRE(empty_part);
  REQUIRE(project.value().add_part_document(empty_part.value()));

  const auto built = AssemblyStepExporter{}.build_posed_shape(project.value());
  REQUIRE(built.has_error());
  CHECK(built.error().category() == ErrorCategory::Geometry);
  CHECK(built.error().object_id() == "part.empty");
  CHECK(built.error().message() == "historical part has no final shape result");
}
