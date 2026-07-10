#include "blcad/core/assembly_document.hpp"
#include "blcad/core/assembly_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

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

Parameter make_count_parameter(const char* id, const char* name, double value,
                               ParameterScope scope = ParameterScope::Part) {
  auto quantity = Quantity::count(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_count(ParameterId(id), name, quantity.value(), scope);
  REQUIRE(parameter);
  return parameter.value();
}

ParameterBinding make_binding(const char* id, const char* part, const char* part_parameter,
                              const char* assembly_parameter) {
  auto binding =
      ParameterBinding::create(ParameterBindingId(id), DocumentId(part),
                               ParameterId(part_parameter), ParameterId(assembly_parameter));
  REQUIRE(binding);
  return binding.value();
}

AssemblyDocument make_flange_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.flange"), "FlangeAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_parameter(make_length_parameter(
      "assembly.bolt_circle_radius", "bolt_circle_radius", 30.0, ParameterScope::Assembly)));
  REQUIRE(assembly.value().add_parameter(
      make_count_parameter("assembly.bolt_count", "bolt_count", 6, ParameterScope::Assembly)));
  return assembly.value();
}

// A part with a bolt-circle pattern whose radius and count should later follow
// the assembly parameters.
PartDocument make_bolt_circle_part(const char* document_id, const char* document_name) {
  auto document = PartDocument::create(DocumentId(document_id), document_name);
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.bolt_circle_radius", "bolt_circle_radius", 25.0)));
  REQUIRE(document.value().add_parameter(make_count_parameter("part.bolt_count", "bolt_count", 4)));
  REQUIRE(document.value().add_parameter(
      make_length_parameter("part.bolt_hole_diameter", "bolt_hole_diameter", 6.6)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(base_sketch.value()));

  auto pattern_sketch =
      Sketch::create(SketchId("sketch.bolt_circle"), "Sketch_BoltCircle", DatumPlaneId("datum.xy"));
  REQUIRE(pattern_sketch);
  auto pattern = CircularHolePattern::create(
      ProfileId("pattern.bolt_circle"), ParameterId("part.bolt_circle_radius"),
      ParameterId("part.bolt_count"), ParameterId("part.bolt_hole_diameter"));
  REQUIRE(pattern);
  REQUIRE(pattern_sketch.value().add_profile(pattern.value()));
  REQUIRE(document.value().add_sketch(pattern_sketch.value()));

  auto base =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base);
  REQUIRE(document.value().add_feature(base.value()));

  auto cut = Feature::create_subtractive_extrude(FeatureId("feature.bolt_circle_cut"),
                                                 "BoltCircleCut", SketchId("sketch.bolt_circle"),
                                                 FeatureId("feature.base_extrude"));
  REQUIRE(cut);
  REQUIRE(document.value().add_feature(cut.value()));

  return document.value();
}

void bind_bolt_circle_part(AssemblyDocument& assembly, const char* part_id,
                           const char* binding_prefix) {
  REQUIRE(assembly.add_member_part(DocumentId(part_id)));
  REQUIRE(
      assembly.add_binding(make_binding((std::string(binding_prefix) + ".radius").c_str(), part_id,
                                        "part.bolt_circle_radius", "assembly.bolt_circle_radius")));
  REQUIRE(assembly.add_binding(make_binding((std::string(binding_prefix) + ".count").c_str(),
                                            part_id, "part.bolt_count", "assembly.bolt_count")));
}

} // namespace

TEST_CASE("AssemblyDocument validates creation and parameter scope") {
  CHECK(AssemblyDocument::create(DocumentId(""), "Flange").has_error());
  CHECK(AssemblyDocument::create(DocumentId("assembly.flange"), "").has_error());

  auto assembly = AssemblyDocument::create(DocumentId("assembly.flange"), "FlangeAssembly");
  REQUIRE(assembly);
  CHECK(assembly.value()
            .add_parameter(make_length_parameter("assembly.radius", "radius", 30.0))
            .has_error());
  CHECK(assembly.value().add_parameter(
      make_length_parameter("assembly.radius", "radius", 30.0, ParameterScope::Assembly)));
  CHECK(assembly.value()
            .add_parameter(make_length_parameter("assembly.radius", "radius_again", 40.0,
                                                 ParameterScope::Assembly))
            .has_error());
}

TEST_CASE("AssemblyDocument validates member parts") {
  auto assembly = make_flange_assembly();
  CHECK(assembly.add_member_part(DocumentId("")).has_error());
  CHECK(assembly.add_member_part(DocumentId("assembly.flange")).has_error());
  REQUIRE(assembly.add_member_part(DocumentId("part.base_plate")));
  CHECK(assembly.add_member_part(DocumentId("part.base_plate")).has_error());
  CHECK(assembly.has_member_part(DocumentId("part.base_plate")));
}

TEST_CASE("AssemblyDocument validates bindings") {
  auto assembly = make_flange_assembly();
  REQUIRE(assembly.add_member_part(DocumentId("part.base_plate")));

  SECTION("part must be a member") {
    CHECK(assembly
              .add_binding(make_binding("binding.radius", "part.unknown", "part.bolt_circle_radius",
                                        "assembly.bolt_circle_radius"))
              .has_error());
  }

  SECTION("assembly parameter must exist") {
    CHECK(assembly
              .add_binding(make_binding("binding.radius", "part.base_plate",
                                        "part.bolt_circle_radius", "assembly.unknown"))
              .has_error());
  }

  SECTION("a part parameter can be bound only once") {
    REQUIRE(assembly.add_binding(make_binding("binding.radius", "part.base_plate",
                                              "part.bolt_circle_radius",
                                              "assembly.bolt_circle_radius")));
    CHECK(assembly
              .add_binding(make_binding("binding.radius_again", "part.base_plate",
                                        "part.bolt_circle_radius", "assembly.bolt_circle_radius"))
              .has_error());
  }
}

TEST_CASE("Applying bindings propagates assembly values into both parts") {
  auto assembly = make_flange_assembly();
  bind_bolt_circle_part(assembly, "part.base_plate", "binding.base");
  bind_bolt_circle_part(assembly, "part.cover_plate", "binding.cover");

  PartDocument base_plate = make_bolt_circle_part("part.base_plate", "BasePlate");
  PartDocument cover_plate = make_bolt_circle_part("part.cover_plate", "CoverPlate");
  base_plate.mark_all_clean();
  cover_plate.mark_all_clean();

  for (PartDocument* part : {&base_plate, &cover_plate}) {
    auto applied = assembly.apply_bindings_to(*part);
    REQUIRE(applied);
    CHECK(applied.value().applied_binding_count == 2U);

    const std::vector<std::string>& nodes = applied.value().affected_part_nodes;
    CHECK(std::find(nodes.begin(), nodes.end(), "sketch.bolt_circle") != nodes.end());
    CHECK(std::find(nodes.begin(), nodes.end(), "feature.bolt_circle_cut") != nodes.end());

    CHECK(part->find_parameter(ParameterId("part.bolt_circle_radius"))->value().millimeters() ==
          30.0);
    CHECK(part->find_parameter(ParameterId("part.bolt_count"))->value().count_value() == 6U);
  }
}

TEST_CASE("Changing an assembly parameter re-propagates on the next application") {
  auto assembly = make_flange_assembly();
  bind_bolt_circle_part(assembly, "part.base_plate", "binding.base");
  PartDocument base_plate = make_bolt_circle_part("part.base_plate", "BasePlate");
  REQUIRE(assembly.apply_bindings_to(base_plate));

  auto new_count = Quantity::count(12.0, "assembly.bolt_count");
  REQUIRE(new_count);
  REQUIRE(assembly.set_parameter_value(ParameterId("assembly.bolt_count"), new_count.value()));

  base_plate.mark_all_clean();
  auto applied = assembly.apply_bindings_to(base_plate);
  REQUIRE(applied);
  CHECK(base_plate.find_parameter(ParameterId("part.bolt_count"))->value().count_value() == 12U);

  const std::vector<std::string>& nodes = applied.value().affected_part_nodes;
  CHECK(std::find(nodes.begin(), nodes.end(), "feature.bolt_circle_cut") != nodes.end());
}

TEST_CASE("Applying bindings rejects non-members, missing parameters, and type mismatches") {
  auto assembly = make_flange_assembly();
  bind_bolt_circle_part(assembly, "part.base_plate", "binding.base");

  SECTION("non-member part") {
    PartDocument other = make_bolt_circle_part("part.other", "Other");
    CHECK(assembly.apply_bindings_to(other).has_error());
  }

  SECTION("missing part parameter") {
    auto part = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
    REQUIRE(part);
    const auto applied = assembly.apply_bindings_to(part.value());
    REQUIRE(applied.has_error());
    CHECK(applied.error().message() ==
          "parameter binding part parameter must exist in part document");
  }

  SECTION("type mismatch") {
    auto part = PartDocument::create(DocumentId("part.base_plate"), "BasePlate");
    REQUIRE(part);
    // part.bolt_circle_radius exists but is a count parameter here.
    REQUIRE(part.value().add_parameter(
        make_count_parameter("part.bolt_circle_radius", "bolt_circle_radius", 3)));
    REQUIRE(part.value().add_parameter(make_count_parameter("part.bolt_count", "bolt_count", 4)));
    const auto applied = assembly.apply_bindings_to(part.value());
    REQUIRE(applied.has_error());
    CHECK(applied.error().message() ==
          "parameter binding requires matching parameter types in assembly and part");
  }
}

TEST_CASE("AssemblyDocument survives JSON roundtrip") {
  auto assembly = make_flange_assembly();
  bind_bolt_circle_part(assembly, "part.base_plate", "binding.base");
  bind_bolt_circle_part(assembly, "part.cover_plate", "binding.cover");

  auto serialized = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized);

  auto restored = deserialize_assembly_document_from_json(serialized.value());
  REQUIRE(restored);

  CHECK(restored.value().id().value() == "assembly.flange");
  CHECK(restored.value().name() == "FlangeAssembly");
  CHECK(restored.value().parameter_count() == 2U);
  CHECK(restored.value().member_part_count() == 2U);
  CHECK(restored.value().binding_count() == 4U);

  const Parameter* count = restored.value().find_parameter(ParameterId("assembly.bolt_count"));
  REQUIRE(count != nullptr);
  CHECK(count->scope() == ParameterScope::Assembly);
  CHECK(count->type() == ParameterType::Count);
  CHECK(count->value().count_value() == 6U);

  const ParameterBinding* binding =
      restored.value().find_binding(ParameterBindingId("binding.cover.count"));
  REQUIRE(binding != nullptr);
  CHECK(binding->part_document().value() == "part.cover_plate");
  CHECK(binding->part_parameter().value() == "part.bolt_count");
  CHECK(binding->assembly_parameter().value() == "assembly.bolt_count");

  // The restored assembly still propagates into a part.
  PartDocument base_plate = make_bolt_circle_part("part.base_plate", "BasePlate");
  auto applied = restored.value().apply_bindings_to(base_plate);
  REQUIRE(applied);
  CHECK(applied.value().applied_binding_count == 2U);
}

TEST_CASE("AssemblyDocument JSON rejects part-scope parameters") {
  const std::string content = R"({
    "schema": "blcad.assembly_document.mvp4",
    "version": 1,
    "document": {"id": "assembly.flange", "name": "FlangeAssembly"},
    "parameters": [
      {"id": "assembly.radius", "name": "radius", "type": "length",
       "scope": "part", "unit": "mm", "value": 30.0}
    ],
    "member_parts": [],
    "parameter_bindings": []
  })";
  const auto restored = deserialize_assembly_document_from_json(content);
  REQUIRE(restored.has_error());
  CHECK(restored.error().message() == "assembly documents support only assembly-scope parameters");
}
