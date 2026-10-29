#pragma once

#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <tuple>

namespace blcad::gui::structured_test {
template <class T> T checked(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}

// Supplier data is authored outside the UI under test, then exchanged through
// the public structured writer. The GUI import never receives this Project.
inline Project supplier_project() {
  auto part = checked(PartDocument::create(DocumentId("supplier.plate"), "Shared plate"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  for (const auto& [id, length] :
       std::vector<std::pair<std::string, double>>{{"width", 20}, {"height", 10}, {"depth", 5}})
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(length))))));
  auto sketch =
      checked(Sketch::create(SketchId("sketch.plate"), "Plate outline", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("profile.plate"), ParameterId("width"), ParameterId("height")))));
  REQUIRE(part.add_sketch(sketch));
  REQUIRE(part.add_body(checked(Body::create(BodyId("body.plate"), "Plate", BodyKind::Solid))));
  auto extrude = checked(Feature::create_additive_extrude(FeatureId("feature.plate"), "Plate",
                                                          sketch.id(), ParameterId("depth")));
  extrude = checked(extrude.with_body_result_context(checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body.plate")))));
  REQUIRE(part.add_feature(extrude));
  const auto component = [&](const char* id, const char* name, RigidTransform pose) {
    return checked(ComponentInstance::create(
        ComponentInstanceId(id), name, part.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Free, pose));
  };
  const auto boundary = [](const char* id, const char* name, DocumentId child,
                           RigidTransform pose) {
    return checked(SubassemblyInstance::create(SubassemblyInstanceId(id), name, child,
                                               ComponentVisibility::Visible,
                                               ComponentSuppressionState::Active, pose));
  };
  auto rotor = checked(AssemblyDocument::create(DocumentId("supplier.rotor"), "Rotor pair"));
  REQUIRE(rotor.add_member_part(part.id()));
  REQUIRE(
      rotor.add_component_instance(component("plate.left", "Plate left", {{0, 0, 0}, {0, 0, 0}})));
  REQUIRE(rotor.add_component_instance(
      component("plate.right", "Plate right", {{25, 0, 0}, {0, 0, 25}})));
  auto carrier = checked(AssemblyDocument::create(DocumentId("supplier.carrier"), "Carrier"));
  REQUIRE(carrier.add_subassembly_instance(
      boundary("rotor.inner", "Inner rotor", rotor.id(), {{0, 12, 4}, {20, 0, 35}})));
  auto assembly =
      checked(AssemblyDocument::create(DocumentId("supplier.root"), "Supplier assembly"));
  REQUIRE(assembly.add_member_part(part.id()));
  REQUIRE(assembly.add_component_instance(
      component("plate.direct", "Direct plate", {{-35, -20, 0}, {0, 0, -20}})));
  REQUIRE(assembly.add_subassembly_instance(
      boundary("carrier.left", "Carrier left", carrier.id(), {{25, 20, 0}, {0, 0, 90}})));
  REQUIRE(assembly.add_subassembly_instance(
      boundary("carrier.right", "Carrier right", carrier.id(), {{-30, 30, 10}, {35, -15, 30}})));
  auto project = checked(Project::create(DocumentId("supplier.project"), "Supplier", assembly));
  REQUIRE(project.add_part_document(part));
  REQUIRE(project.add_child_assembly_document(rotor));
  REQUIRE(project.add_child_assembly_document(carrier));
  REQUIRE(project.validate_assembly_structure());
  return project;
}

inline void write_supplier(const std::string& path) {
  const auto result =
      checked(geometry::AssemblyStructuredStepExporter{}.write_step(supplier_project(), path));
  REQUIRE(result.part_product_definition_count == 1);
  REQUIRE(result.exported_component_occurrence_count == 5);
}

inline void check_source_structure(const Project& project,
                                   const geometry::StepImportGeometry& imported) {
  std::vector<std::string> source_parts, source_assemblies, actual_parts, actual_assemblies;
  for (const auto& product : imported.products)
    (product.is_assembly ? source_assemblies : source_parts).push_back(product.name);
  for (const auto& part : project.part_documents())
    actual_parts.push_back(part.name());
  for (const auto& assembly : project.child_assembly_documents())
    actual_assemblies.push_back(assembly.name());
  for (auto* names : {&source_parts, &source_assemblies, &actual_parts, &actual_assemblies})
    std::sort(names->begin(), names->end());
  CHECK(actual_parts == source_parts);
  CHECK(actual_assemblies == source_assemblies);
  // The public supplier exporter writes an Assembly product per rooted
  // occurrence. Import preserves those actual STEP definitions; it must not
  // coalesce them back to the supplier's original BLCAD definition count.
  CHECK(project.child_assembly_document_count() == source_assemblies.size());
  struct Occurrence {
    std::string parent, product, name;
    bool assembly;
    std::array<double, 12> matrix;
  };
  std::vector<Occurrence> expected, actual;
  for (const auto& occurrence : imported.occurrences) {
    REQUIRE(occurrence.product_definition < imported.products.size());
    if (occurrence.parent_definition)
      REQUIRE(*occurrence.parent_definition < imported.products.size());
    const auto& product = imported.products[occurrence.product_definition];
    expected.push_back(
        {occurrence.parent_definition ? imported.products[*occurrence.parent_definition].name
                                      : "<root>",
         product.name, occurrence.name, product.is_assembly, occurrence.transform.matrix});
  }
  const auto matrix = [](const RigidTransform& pose) {
    const geometry::AssemblyHierarchyTransformEvaluator evaluator;
    const auto origin = evaluator.evaluate_point({pose}, {});
    const auto x = evaluator.evaluate_vector({pose}, {1, 0, 0});
    const auto y = evaluator.evaluate_vector({pose}, {0, 1, 0});
    const auto z = evaluator.evaluate_vector({pose}, {0, 0, 1});
    return std::array<double, 12>{x.x, y.x,      z.x, origin.x, x.y, y.y,
                                  z.y, origin.y, x.z, y.z,      z.z, origin.z};
  };
  const auto append = [&](const AssemblyDocument& assembly, const std::string& parent) {
    for (const auto& occurrence : assembly.component_instances()) {
      const auto* part = project.find_part_document(occurrence.referenced_part_document());
      REQUIRE(part);
      actual.push_back(
          {parent, part->name(), occurrence.name(), false, matrix(occurrence.transform())});
    }
    for (const auto& occurrence : assembly.subassembly_instances()) {
      const auto* child = project.find_assembly_document(occurrence.referenced_assembly_document());
      REQUIRE(child);
      actual.push_back(
          {parent, child->name(), occurrence.name(), true, matrix(occurrence.transform())});
    }
  };
  append(project.assembly(), "<root>");
  for (const auto& assembly : project.child_assembly_documents())
    append(assembly, assembly.name());
  const auto order = [](const Occurrence& a, const Occurrence& b) {
    return std::tie(a.parent, a.product, a.name, a.assembly, a.matrix) <
           std::tie(b.parent, b.product, b.name, b.assembly, b.matrix);
  };
  std::sort(expected.begin(), expected.end(), order);
  std::sort(actual.begin(), actual.end(), order);
  REQUIRE(actual.size() == expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    INFO("Source occurrence " << expected[i].parent << '/' << expected[i].name);
    CHECK(actual[i].parent == expected[i].parent);
    CHECK(actual[i].product == expected[i].product);
    CHECK(actual[i].name == expected[i].name);
    CHECK(actual[i].assembly == expected[i].assembly);
    for (std::size_t entry = 0; entry < expected[i].matrix.size(); ++entry)
      CHECK(actual[i].matrix[entry] == Catch::Approx(expected[i].matrix[entry]).margin(1e-7));
  }
}
} // namespace blcad::gui::structured_test
