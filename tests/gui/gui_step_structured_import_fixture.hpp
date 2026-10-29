#pragma once

#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_structured_step_exporter.hpp"
#include "blcad/geometry/measure_selection.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
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

struct PhysicalBody {
  double volume;
  std::array<double, 6> vertex_bounds;
  std::size_t solids;
};

// These acceptance fixtures have planar outer box boundaries. Their actual
// corner vertices therefore give an independent exact footprint, including
// noncommuting occurrence poses; no OCCT headers are needed by GUI tests.
inline std::vector<PhysicalBody>
physical_bodies(const std::vector<geometry::GeometryShape>& shapes) {
  std::vector<PhysicalBody> result;
  for (const auto& shape : shapes) {
    const auto summary = geometry::RectangleExtrusionAdapter{}.summarize(shape);
    REQUIRE_FALSE(summary.is_null);
    REQUIRE(summary.solid_count >= 1);
    const auto points = checked(geometry::MeasureSelectionBuilder::build(
        {{"acceptance.body", "Acceptance body", shape, {}}}, 1));
    const auto infinity = std::numeric_limits<double>::infinity();
    std::array<double, 6> bounds{infinity, infinity, infinity, -infinity, -infinity, -infinity};
    std::size_t vertices = 0;
    for (const auto& point : points) {
      if (point.kind != geometry::MeasureSelectionKind::Point)
        continue;
      ++vertices;
      const std::array xyz{point.anchor.x, point.anchor.y, point.anchor.z};
      for (std::size_t axis = 0; axis < 3; ++axis) {
        bounds[axis] = std::min(bounds[axis], xyz[axis]);
        bounds[axis + 3] = std::max(bounds[axis + 3], xyz[axis]);
      }
    }
    REQUIRE(vertices >= 4);
    result.push_back({summary.volume_mm3, bounds, summary.solid_count});
  }
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    return std::tie(a.vertex_bounds, a.volume, a.solids) <
           std::tie(b.vertex_bounds, b.volume, b.solids);
  });
  return result;
}

inline std::vector<geometry::GeometryShape> project_shapes(const Project& project,
                                                           const std::filesystem::path& root) {
  geometry::StepImportTopologyContext context(root);
  const auto scene = checked(geometry::ViewportSceneBuilder{&context}.build_project(project));
  REQUIRE(context.validate_sources());
  std::vector<geometry::GeometryShape> result;
  for (const auto& item : scene)
    if (item.kind == geometry::ViewportSceneKind::Component)
      result.push_back(item.shape);
  return result;
}

inline std::vector<geometry::GeometryShape> read_export(const std::filesystem::path& path) {
  const auto observed =
      checked(inspect_step_import_asset(path.parent_path(), path.filename().string()));
  REQUIRE(observed.observed_sha256_digest());
  const auto source = checked(StepImportSource::create(
      StepImportSourceId("source.export.acceptance"), "Export readback", path.filename().string(),
      *observed.observed_sha256_digest(), StepImportMode::Reference));
  const auto imported = checked(geometry::StepImportReader{}.read(source, path.parent_path()));
  return checked(geometry::StepImportReader{}.preview_shapes(imported));
}

inline void check_export(const std::filesystem::path& path,
                         const std::vector<geometry::GeometryShape>& expected) {
  const auto before = physical_bodies(expected);
  const auto after = physical_bodies(read_export(path));
  REQUIRE(after.size() == before.size());
  for (std::size_t index = 0; index < before.size(); ++index) {
    INFO("Exported physical body " << index << " in " << path.string());
    CHECK(after[index].solids == before[index].solids);
    CHECK(after[index].volume == Catch::Approx(before[index].volume).margin(1e-4));
    for (std::size_t axis = 0; axis < 6; ++axis)
      CHECK(after[index].vertex_bounds[axis] ==
            Catch::Approx(before[index].vertex_bounds[axis]).margin(1e-5));
  }
}

inline void check_portable_json(const std::string& text) {
  const auto json = nlohmann::json::parse(text);
  const std::set<std::string> runtime_keys{"definition_key",
                                           "parent_definition",
                                           "product_definition",
                                           "shape",
                                           "native_shape",
                                           "TopoDS_Shape",
                                           "xde_label",
                                           "step_entity",
                                           "runtime_product_index",
                                           "captured_product_index",
                                           "preview_shapes",
                                           "document_path",
                                           "document_json",
                                           "asset_root",
                                           "root_directory"};
  std::vector<std::string> unsupported;
  const auto check = [&](auto&& self, const nlohmann::json& value,
                         const std::string& path) -> void {
    if (value.is_object())
      for (const auto& [key, child] : value.items()) {
        if (runtime_keys.contains(key))
          unsupported.push_back(path + '/' + key);
        self(self, child, path + '/' + key);
      }
    else if (value.is_array())
      for (std::size_t index = 0; index < value.size(); ++index)
        self(self, value[index], path + '/' + std::to_string(index));
  };
  check(check, json, "");
  CAPTURE(unsupported);
  CHECK(unsupported.empty());
}
} // namespace blcad::gui::structured_test
