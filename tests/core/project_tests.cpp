#include "blcad/core/project.hpp"
#include "blcad/core/project_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm,
                                ParameterScope scope = ParameterScope::Part) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value(), scope);
  REQUIRE(parameter);
  return parameter.value();
}

ParameterBinding make_binding(const char* id, const char* part, const char* part_parameter,
                              const char* assembly_parameter) {
  auto binding = ParameterBinding::create(ParameterBindingId(id), DocumentId(part),
                                          ParameterId(part_parameter),
                                          ParameterId(assembly_parameter));
  REQUIRE(binding);
  return binding.value();
}

AssemblyDocument make_width_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.width"), "WidthAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_parameter(
      make_length_parameter("assembly.width", "width", 100.0, ParameterScope::Assembly)));
  return assembly.value();
}

PartDocument make_bound_plate_part(const char* document_id, const char* name, double width_mm) {
  auto document = PartDocument::create(DocumentId(document_id), name);
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", width_mm)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 30.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 5.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto sketch = Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto feature = Feature::create_additive_extrude(FeatureId("feature.base_extrude"),
                                                  "BaseExtrude", SketchId("sketch.base"),
                                                  ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

Project make_bound_project() {
  auto assembly = make_width_assembly();
  REQUIRE(assembly.add_member_part(DocumentId("part.base_plate")));
  REQUIRE(assembly.add_member_part(DocumentId("part.cover_plate")));
  REQUIRE(assembly.add_binding(make_binding("binding.base.width", "part.base_plate", "part.width",
                                            "assembly.width")));
  REQUIRE(assembly.add_binding(make_binding("binding.cover.width", "part.cover_plate", "part.width",
                                            "assembly.width")));

  auto project = Project::create(DocumentId("project.flange"), "FlangeProject", std::move(assembly));
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_bound_plate_part("part.base_plate", "BasePlate", 40.0)));
  REQUIRE(project.value().add_part_document(make_bound_plate_part("part.cover_plate", "CoverPlate", 45.0)));
  return project.value();
}

} // namespace

TEST_CASE("Project validates assembly member ownership", "[core][project]") {
  auto assembly = make_width_assembly();
  REQUIRE(assembly.add_member_part(DocumentId("part.base_plate")));

  auto project = Project::create(DocumentId("project.flange"), "FlangeProject", std::move(assembly));
  REQUIRE(project);
  CHECK(project.value().validate_member_parts().has_error());

  REQUIRE(project.value().add_part_document(make_bound_plate_part("part.base_plate", "BasePlate", 40.0)));
  CHECK(project.value().validate_member_parts());
  CHECK(project.value().add_part_document(make_bound_plate_part("part.base_plate", "Duplicate", 45.0))
            .has_error());
}

TEST_CASE("Project applies assembly parameter updates and exposes per-part recompute plans",
          "[core][project]") {
  auto project = make_bound_project();
  for (PartDocument& part : project.part_documents()) {
    part.mark_all_clean();
  }

  auto new_width = Quantity::length_mm(120.0, "assembly.width");
  REQUIRE(new_width);
  auto update = project.set_assembly_parameter_value(ParameterId("assembly.width"), new_width.value());
  REQUIRE(update);
  REQUIRE(update.value().updated_part_count() == 2U);

  for (const ProjectPartUpdate& part_update : update.value().part_updates()) {
    CHECK(part_update.binding_application().applied_binding_count == 1U);
    CHECK_FALSE(part_update.recompute_plan().empty());
    CHECK(part_update.recompute_plan().contains("feature.base_extrude"));

    const PartDocument* part = project.find_part_document(part_update.part_document());
    REQUIRE(part != nullptr);
    const Parameter* width = part->find_parameter(ParameterId("part.width"));
    REQUIRE(width != nullptr);
    CHECK(width->value().millimeters() == 120.0);
  }
}

TEST_CASE("Project JSON roundtrip preserves assembly and owned parts", "[core][project]") {
  auto project = make_bound_project();
  auto serialized = serialize_project_to_json(project);
  REQUIRE(serialized);

  auto restored = deserialize_project_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().id().value() == "project.flange");
  CHECK(restored.value().name() == "FlangeProject");
  CHECK(restored.value().assembly().member_part_count() == 2U);
  CHECK(restored.value().part_document_count() == 2U);
  CHECK(restored.value().validate_member_parts());

  auto new_width = Quantity::length_mm(90.0, "assembly.width");
  REQUIRE(new_width);
  auto update = restored.value().set_assembly_parameter_value(ParameterId("assembly.width"),
                                                              new_width.value());
  REQUIRE(update);
  CHECK(update.value().updated_part_count() == 2U);
}

TEST_CASE("Project JSON rejects assemblies with missing owned member parts", "[core][project]") {
  const std::string content = R"({
    "schema": "blcad.project.mvp4",
    "version": 1,
    "project": {"id": "project.invalid", "name": "InvalidProject"},
    "assembly": {
      "schema": "blcad.assembly_document.mvp4",
      "version": 1,
      "document": {"id": "assembly.width", "name": "WidthAssembly"},
      "parameters": [
        {"id": "assembly.width", "name": "width", "type": "length", "scope": "assembly", "unit": "mm", "value": 100.0}
      ],
      "member_parts": ["part.missing"],
      "parameter_bindings": []
    },
    "parts": []
  })";

  auto restored = deserialize_project_from_json(content);
  REQUIRE(restored.has_error());
  CHECK(restored.error().message() ==
        "assembly member part must resolve to an owned project part document");
}
