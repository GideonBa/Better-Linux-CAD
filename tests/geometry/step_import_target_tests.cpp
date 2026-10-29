#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/recompute_plan.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"

#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_Controller.hxx>
#include <STEPControl_Writer.hxx>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {
template <typename T> T checked(Result<T> result) {
  if (!result)
    FAIL(result.error().message());
  return std::move(result.value());
}
struct Directory {
  std::filesystem::path root;
  Directory() {
    char name[] = "/tmp/blcad-import-targets-XXXXXX";
    const auto* created = ::mkdtemp(name);
    REQUIRE(created);
    root = created;
  }
  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
};
struct CurrentDirectory {
  std::filesystem::path original = std::filesystem::current_path();
  ~CurrentDirectory() {
    std::error_code error;
    std::filesystem::current_path(original, error);
  }
};
std::string bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  REQUIRE(stream.good());
  return {std::istreambuf_iterator<char>(stream), {}};
}
void write(const std::filesystem::path& path, const std::string& value) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream << value;
  REQUIRE(stream.good());
}
struct Supplier {
  StepImportSource source;
  ImportedTopologyCatalog catalog;
};
Supplier capture(const Directory& directory, const char* file = "supplier.step") {
  const auto inspected = checked(inspect_step_import_asset(directory.root, file));
  REQUIRE(inspected.observed_sha256_digest());
  auto source = checked(StepImportSource::create(StepImportSourceId("supplier"), "Supplier", file,
                                                 *inspected.observed_sha256_digest(),
                                                 StepImportMode::EditableBody));
  auto imported = checked(StepImportReader{}.read(source, directory.root));
  REQUIRE(imported.products.size() == 1);
  auto catalog = checked(
      StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog"), imported, 0));
  return {std::move(source), std::move(catalog)};
}
Supplier cylinder(const Directory& directory) {
  STEPControl_Controller::Init();
  const std::string previous = Interface_Static::CVal("write.step.unit");
  REQUIRE(Interface_Static::SetCVal("write.step.unit", "MM"));
  STEPControl_Writer writer;
  const auto transferred =
      writer.Transfer(BRepPrimAPI_MakeCylinder(5, 30).Shape(), STEPControl_AsIs);
  Interface_Static::SetCVal("write.step.unit", previous.c_str());
  REQUIRE(transferred == IFSelect_RetDone);
  REQUIRE(writer.Write((directory.root / "supplier.step").c_str()) == IFSelect_RetDone);
  return capture(directory);
}
PartDocument part(const char* id, const Supplier& supplier) {
  auto document = checked(PartDocument::create(DocumentId(id), id));
  REQUIRE(document.add_step_import_source(supplier.source));
  REQUIRE(document.add_imported_topology_catalog(supplier.catalog));
  return document;
}
ImportedTopologyReference reference(
    const ImportedTopologyCatalog& catalog, ImportedGeometryClass geometry,
    ImportedTopologyCapability capability,
    const std::function<bool(const ImportedTopologyDescriptor&)>& predicate = [](const auto&) {
      return true;
    }) {
  const auto found =
      std::find_if(catalog.entries().begin(), catalog.entries().end(), [&](const auto& entry) {
        return entry.descriptor.geometry == geometry && predicate(entry.descriptor);
      });
  REQUIRE(found != catalog.entries().end());
  return checked(ImportedTopologyReference::create(catalog.id(), found->id, capability));
}
void point_is(Point3 actual, Point3 expected) {
  CHECK(actual.x == Approx(expected.x).margin(1e-8));
  CHECK(actual.y == Approx(expected.y).margin(1e-8));
  CHECK(actual.z == Approx(expected.z).margin(1e-8));
}
void vector_is(Vector3 actual, Vector3 expected) {
  CHECK(actual.x == Approx(expected.x).margin(1e-8));
  CHECK(actual.y == Approx(expected.y).margin(1e-8));
  CHECK(actual.z == Approx(expected.z).margin(1e-8));
}
PartDocument box_document() {
  auto document = checked(PartDocument::create(DocumentId("authored"), "Authored box"));
  REQUIRE(document.add_datum_plane(checked(DatumPlane::xy())));
  for (const auto& [name, value] : std::vector<std::pair<std::string, double>>{
           {"width", 4}, {"height", 6}, {"depth", 10}, {"radius", 0.5}})
    REQUIRE(document.add_parameter(checked(
        Parameter::create_length(ParameterId(name), name, checked(Quantity::length_mm(value))))));
  auto sketch = checked(Sketch::create(SketchId("sketch"), "Profile", DatumPlaneId("datum.xy")));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("profile"), ParameterId("width"), ParameterId("height"), {15, 2}))));
  REQUIRE(document.add_sketch(sketch));
  REQUIRE(document.add_body(checked(Body::create(BodyId("body"), "Body", BodyKind::Solid))));
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("extrude"), "Extrude", SketchId("sketch"), ParameterId("depth")));
  feature = checked(feature.with_body_result_context(checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body")))));
  REQUIRE(document.add_feature(feature));
  return document;
}
Supplier export_box(const Directory& directory, const PartDocument& document) {
  auto cache = checked(ShapeCache::create(ShapeCacheId("supplier")));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  REQUIRE(cache.find_body_shape(BodyId("body")));
  REQUIRE(StepExporter{}.write_step(*cache.find_body_shape(BodyId("body")),
                                    (directory.root / "supplier.step").string()));
  return capture(directory);
}
Project project_with_occurrences(const PartDocument& document) {
  const RigidTransform component{{5, 3, 11}, {90, 0, 0}};
  auto root = checked(AssemblyDocument::create(DocumentId("assembly.root"), "Root"));
  REQUIRE(root.add_member_part(document.id()));
  REQUIRE(root.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("local"), "Local", document.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, component))));
  REQUIRE(root.add_subassembly_instance(checked(SubassemblyInstance::create(
      SubassemblyInstanceId("outer"), "Outer", DocumentId("assembly.middle"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      RigidTransform{{-10, 4, 1}, {0, 90, 0}}))));
  auto middle = checked(AssemblyDocument::create(DocumentId("assembly.middle"), "Middle"));
  REQUIRE(middle.add_subassembly_instance(checked(SubassemblyInstance::create(
      SubassemblyInstanceId("inner"), "Inner", DocumentId("assembly.leaf"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      RigidTransform{{100, -20, 7}, {0, 0, 90}}))));
  auto leaf = checked(AssemblyDocument::create(DocumentId("assembly.leaf"), "Leaf"));
  REQUIRE(leaf.add_member_part(document.id()));
  REQUIRE(leaf.add_component_instance(checked(ComponentInstance::create(
      ComponentInstanceId("nested"), "Nested", document.id(), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, component))));
  auto project = checked(Project::create(DocumentId("project"), "Project", root));
  REQUIRE(project.add_part_document(document));
  REQUIRE(project.add_child_assembly_document(middle));
  REQUIRE(project.add_child_assembly_document(leaf));
  REQUIRE(project.validate_assembly_structure());
  return project;
}
} // namespace

TEST_CASE("Imported topology context enforces owned snapshots and source freshness",
          "[geometry][step-import-topology-recovery]") {
  Directory directory;
  const auto supplier = cylinder(directory);
  const auto document = part("source", supplier);
  const auto plane =
      reference(supplier.catalog, ImportedGeometryClass::Plane, ImportedTopologyCapability::Plane);
  StepImportTopologyContext context(directory.root);
  REQUIRE(context.resolve(document, plane));
  CHECK(context.validate_sources());
  const auto original = bytes(directory.root / "supplier.step");
  write(directory.root / "supplier.step", original + "\n/* changed */\n");
  CHECK_FALSE(context.resolve(document, plane));
  CHECK_FALSE(context.validate_sources());
  write(directory.root / "supplier.step", original);
  REQUIRE(context.resolve(document, plane));
  auto other = checked(PartDocument::create(document.id(), "Different snapshot"));
  REQUIRE(other.add_step_import_source(checked(StepImportSource::create(
      supplier.source.id(), "Changed source intent", supplier.source.project_relative_asset_path(),
      supplier.source.sha256_digest(), supplier.source.mode()))));
  REQUIRE(other.add_imported_topology_catalog(supplier.catalog));
  CHECK_FALSE(context.resolve(other, plane));
  StepImportTopologyContext new_context(directory.root);
  CHECK(new_context.resolve(other, plane));
  const auto unrelated = checked(PartDocument::create(DocumentId("unrelated"), "Unrelated"));
  CHECK_FALSE(new_context.resolve(unrelated, plane));
  std::filesystem::remove(directory.root / "supplier.step");
  CHECK_FALSE(new_context.resolve(other, plane));
}

TEST_CASE("Imported topology context binds a relative asset root before cwd changes",
          "[geometry][step-import-topology-recovery][step-import-root-context]") {
  Directory saved, replacement;
  const auto supplier = cylinder(saved);
  const auto document = part("source", supplier);
  const auto plane =
      reference(supplier.catalog, ImportedGeometryClass::Plane, ImportedTopologyCapability::Plane);
  const auto original = bytes(saved.root / "supplier.step");
  const auto relative_root = saved.root.filename();
  REQUIRE(std::filesystem::create_directories(replacement.root / relative_root));
  write(replacement.root / relative_root / "supplier.step", original);
  CurrentDirectory restore_cwd;
  std::filesystem::current_path(saved.root.parent_path());
  StepImportTopologyContext context(relative_root);
  SECTION("bind before the first source read") {}
  SECTION("bind across cached source reads") {
    REQUIRE(context.resolve(document, plane));
  }
  std::filesystem::current_path(replacement.root);
  REQUIRE(context.resolve(document, plane));
  write(saved.root / "supplier.step", original + "\n/* changed saved source */\n");
  CHECK_FALSE(context.validate_sources());
  CHECK_FALSE(context.resolve(document, plane));
  write(saved.root / "supplier.step", original);
  write(replacement.root / relative_root / "supplier.step", "replacement directory is unrelated");
  CHECK(context.validate_sources());
  CHECK(context.resolve(document, plane));
  StepImportTopologyContext empty_root(std::filesystem::path{});
  CHECK_FALSE(empty_root.resolve(document, plane));
}

TEST_CASE("Imported planar reference drives real authored mirror recompute and JSON reopening",
          "[geometry][step-import-topology-recovery][step-import-part-input]") {
  Directory directory;
  auto document = box_document();
  const auto supplier = export_box(directory, document);
  const auto original = bytes(directory.root / "supplier.step");
  REQUIRE(document.add_step_import_source(supplier.source));
  REQUIRE(document.add_imported_topology_catalog(supplier.catalog));
  const auto bottom =
      reference(supplier.catalog, ImportedGeometryClass::Plane, ImportedTopologyCapability::Plane,
                [](const auto& d) {
                  return std::abs(d.direction.z - 1) < 1e-8 && std::abs(d.origin.z) < 1e-8;
                });
  REQUIRE(
      document.add_body(checked(Body::create(BodyId("mirrored"), "Mirrored", BodyKind::Solid))));
  REQUIRE(document.add_mirror_feature(checked(MirrorFeature::create(
      FeatureId("mirror"), "Mirror", {checked(MirrorSourceReference::body(BodyId("body")))},
      checked(PlaneReference::create_imported(PartFeatureInputRole::MirrorPlane, bottom)),
      checked(FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                               BodyId("mirrored")))))));
  const auto json = checked(serialize_part_document_to_json(document));
  auto cache = checked(ShapeCache::create(ShapeCacheId("mirror")));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  CHECK(cache.find_body_shape(BodyId("mirrored")) == nullptr);
  StepImportTopologyContext context(directory.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, cache));
  const auto* mirrored = cache.find_body_shape(BodyId("mirrored"));
  REQUIRE(mirrored);
  CHECK(RectangleExtrusionAdapter{}.summarize(*mirrored).volume_mm3 == Approx(240));
  // Inspect geometric bounds directly; BodyTransformAdapter::bounds intentionally includes
  // OCCT's shape-tolerance gap and is a conservative broad-phase box.
  const auto* mirrored_shape = OcctShapeView::get(*mirrored);
  REQUIRE(mirrored_shape);
  Bnd_Box bounds;
  BRepBndLib::AddOptimal(*mirrored_shape, bounds, false, false);
  double xmin, ymin, zmin, xmax, ymax, zmax;
  bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
  CHECK(zmin == Approx(-10).margin(1e-7));
  CHECK(zmax == Approx(0).margin(1e-7));
  auto reopened = checked(deserialize_part_document_from_json(json));
  auto reopened_cache = checked(ShapeCache::create(ShapeCacheId("reopened")));
  StepImportTopologyContext reopened_context(directory.root);
  REQUIRE(GeometryRecomputeExecutor{&reopened_context}.execute_document(reopened, reopened_cache));
  REQUIRE(reopened_cache.find_body_shape(BodyId("mirrored")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*reopened_cache.find_body_shape(BodyId("mirrored")))
            .volume_mm3 == Approx(240));
  CHECK(checked(serialize_part_document_to_json(document)) == json);
  CHECK(bytes(directory.root / "supplier.step") == original);
  write(directory.root / "supplier.step", original + "\n/* changed */\n");
  CHECK_FALSE(GeometryRecomputeExecutor{&context}.execute_document(document, cache));
  CHECK(cache.find_body_shape(BodyId("mirrored")) == mirrored);
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*cache.find_body_shape(BodyId("mirrored")))
            .volume_mm3 == Approx(240));
}

TEST_CASE("Imported axis inputs and full trimmed edge references feed existing Part consumers",
          "[geometry][step-import-topology-recovery][step-import-part-input]") {
  Directory directory;
  SECTION("cylindrical and circular axes and linear directions") {
    const auto supplier = cylinder(directory);
    const auto document = part("source", supplier);
    StepImportTopologyContext context(directory.root);
    auto cache = checked(ShapeCache::create(ShapeCacheId("axes")));
    for (const auto geometry : {ImportedGeometryClass::Cylinder, ImportedGeometryClass::Circle,
                                ImportedGeometryClass::Line}) {
      const auto cap = geometry == ImportedGeometryClass::Line ? ImportedTopologyCapability::Line
                                                               : ImportedTopologyCapability::Axis;
      const auto imported = reference(supplier.catalog, geometry, cap);
      if (geometry == ImportedGeometryClass::Line)
        CHECK_FALSE(AxisReference::create_imported(PartFeatureInputRole::RevolveAxis, imported));
      const auto role = geometry == ImportedGeometryClass::Line
                            ? PartFeatureInputRole::DraftPullDirection
                            : PartFeatureInputRole::RevolveAxis;
      const auto axis = checked(AxisReference::create_imported(role, imported));
      const auto actual =
          checked(GeometryRecomputeExecutor{&context}.resolve_feature_axis(document, axis, cache));
      vector_is(actual.second, {0, 0, 1});
      CHECK(actual.first.x == Approx(geometry == ImportedGeometryClass::Line ? 5 : 0).margin(1e-7));
      CHECK_FALSE(GeometryRecomputeExecutor{}.resolve_feature_axis(document, axis, cache));
    }
  }
  SECTION("imported edge finishes the matching authored target") {
    auto document = box_document();
    const auto supplier = export_box(directory, document);
    REQUIRE(document.add_step_import_source(supplier.source));
    REQUIRE(document.add_imported_topology_catalog(supplier.catalog));
    const auto imported =
        reference(supplier.catalog, ImportedGeometryClass::Line, ImportedTopologyCapability::Edge,
                  [](const auto& d) { return std::abs(d.direction.z - 1) < 1e-8; });
    const auto edge =
        checked(EdgeReference::create_imported(PartFeatureInputRole::FilletEdge, imported));
    REQUIRE(document.add_fillet_feature(checked(FilletFeature::create(
        FeatureId("fillet"), "Fillet", BodyId("body"), {edge}, ParameterId("radius")))));
    auto cache = checked(ShapeCache::create(ShapeCacheId("fillet")));
    StepImportTopologyContext context(directory.root);
    REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, cache));
    REQUIRE(cache.find_body_shape(BodyId("body")));
    CHECK(RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("body"))).volume_mm3 <
          240);
  }
}

TEST_CASE(
    "Imported planar faces bound real ToFace and Between extrusions atomically",
    "[geometry][step-import-topology-recovery][step-import-part-input][step-import-face-limits]") {
  Directory directory;
  auto document = box_document();
  const auto supplier = export_box(directory, document);
  const auto original = bytes(directory.root / "supplier.step");
  REQUIRE(document.add_step_import_source(supplier.source));
  REQUIRE(document.add_imported_topology_catalog(supplier.catalog));
  const auto limit = [&](double height) {
    return checked(FaceReference::create_imported(
        PartFeatureInputRole::ExtrudeLimitFace,
        reference(supplier.catalog, ImportedGeometryClass::Plane, ImportedTopologyCapability::Face,
                  [height](const auto& descriptor) {
                    return std::abs(descriptor.direction.z - 1) < 1e-8 &&
                           std::abs(descriptor.origin.z - height) < 1e-8;
                  })));
  };
  auto extent = checked(ExtrudeExtentIntent::to_face(limit(10)));
  SECTION("ToFace uses the imported top plane") {}
  SECTION("Between uses both imported planes") {
    extent = checked(ExtrudeExtentIntent::between(limit(0), limit(10)));
  }
  REQUIRE(document.add_body(checked(Body::create(BodyId("limited"), "Limited", BodyKind::Solid))));
  auto feature = checked(Feature::create_additive_extrude(
      FeatureId("import.limited.extrude"), "Imported limits", SketchId("sketch"),
      checked(ExtrudeFeatureIntent::create(extent))));
  feature = checked(feature.with_body_result_context(checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("limited")))));
  REQUIRE(document.add_feature(feature));
  const auto json = checked(serialize_part_document_to_json(document));
  auto cache = checked(ShapeCache::create(ShapeCacheId("import.limits")));
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(document, cache));
  CHECK(cache.body_shape_count() == 0);
  StepImportTopologyContext context(directory.root);
  REQUIRE(GeometryRecomputeExecutor{&context}.execute_document(document, cache));
  const auto* result = cache.find_body_shape(BodyId("limited"));
  REQUIRE(result);
  CHECK(RectangleExtrusionAdapter{}.summarize(*result).volume_mm3 == Approx(240));
  CHECK(bytes(directory.root / "supplier.step") == original);
  auto reopened = checked(deserialize_part_document_from_json(json));
  auto reopened_cache = checked(ShapeCache::create(ShapeCacheId("import.limits.reopened")));
  StepImportTopologyContext reopened_context(directory.root);
  REQUIRE(GeometryRecomputeExecutor{&reopened_context}.execute_document(reopened, reopened_cache));
  REQUIRE(reopened_cache.find_body_shape(BodyId("limited")));
  CHECK(RectangleExtrusionAdapter{}
            .summarize(*reopened_cache.find_body_shape(BodyId("limited")))
            .volume_mm3 == Approx(240));
  write(directory.root / "supplier.step", original + "\n/* changed limit source */\n");
  CHECK_FALSE(GeometryRecomputeExecutor{&context}.execute_document(document, cache));
  CHECK(cache.find_body_shape(BodyId("limited")) == result);
  CHECK(
      RectangleExtrusionAdapter{}.summarize(*cache.find_body_shape(BodyId("limited"))).volume_mm3 ==
      Approx(240));
  CHECK(checked(serialize_part_document_to_json(document)) == json);
}

TEST_CASE(
    "Legacy imported face limits retain prior caches after failed full and incremental recompute",
    "[geometry][step-import-topology-recovery][step-import-part-input][step-import-legacy-"
    "atomicity]") {
  Directory directory;
  const auto authored = box_document();
  const auto supplier = export_box(directory, authored);
  const auto original = bytes(directory.root / "supplier.step");
  auto document = part("legacy.import.limits", supplier);
  for (const auto& parameter : authored.parameters())
    REQUIRE(document.add_parameter(parameter));
  REQUIRE(document.add_datum_plane(checked(DatumPlane::xy())));
  REQUIRE(document.add_sketch(*authored.find_sketch(SketchId("sketch"))));
  const auto face = checked(FaceReference::create_imported(
      PartFeatureInputRole::ExtrudeLimitFace,
      reference(supplier.catalog, ImportedGeometryClass::Plane, ImportedTopologyCapability::Face,
                [](const auto& descriptor) {
                  return std::abs(descriptor.direction.z - 1) < 1e-8 &&
                         std::abs(descriptor.origin.z - 10) < 1e-8;
                })));
  const FeatureId feature("legacy.import.to_face");
  REQUIRE(document.add_feature(checked(Feature::create_additive_extrude(
      feature, "Legacy imported limit", SketchId("sketch"),
      checked(ExtrudeFeatureIntent::create(checked(ExtrudeExtentIntent::to_face(face))))))));
  REQUIRE(document.bodies().empty());
  const auto plan = checked(
      RecomputePlan::from_graph_and_changed_nodes(document.dependency_graph(), {feature.value()}));
  REQUIRE(plan.contains(feature.value()));
  bool incremental = false;
  SECTION("full document recompute") {}
  SECTION("incremental plan recompute") {
    incremental = true;
  }
  const auto execute = [&](const GeometryRecomputeExecutor& executor, ShapeCache& cache) {
    return incremental ? executor.execute_plan(document, plan, cache)
                       : executor.execute_document(document, cache);
  };
  StepImportTopologyContext context(directory.root);
  auto prior = checked(ShapeCache::create(ShapeCacheId("legacy.import.prior")));
  REQUIRE(execute(GeometryRecomputeExecutor{&context}, prior));
  REQUIRE(prior.find_feature_shape(feature));
  REQUIRE(prior.has_final_shape());
  CHECK(RectangleExtrusionAdapter{}.summarize(*prior.find_feature_shape(feature)).volume_mm3 ==
        Approx(240));
  for (const bool stale_source : {false, true}) {
    CAPTURE(incremental, stale_source);
    auto candidate = prior;
    const auto* previous_shape = candidate.find_feature_shape(feature);
    const auto* previous_final = candidate.final_shape();
    const auto feature_count = candidate.feature_shape_count();
    const auto body_count = candidate.body_shape_count();
    if (stale_source)
      write(directory.root / "supplier.step", original + "\n/* stale legacy limit */\n");
    CHECK_FALSE(
        execute(stale_source ? GeometryRecomputeExecutor{&context} : GeometryRecomputeExecutor{},
                candidate));
    CHECK(candidate.feature_shape_count() == feature_count);
    CHECK(candidate.body_shape_count() == body_count);
    CHECK(candidate.find_feature_shape(feature) == previous_shape);
    CHECK(candidate.final_shape() == previous_final);
    if (const auto* shape = candidate.find_feature_shape(feature))
      CHECK(RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3 == Approx(240));
    if (candidate.final_shape())
      CHECK(RectangleExtrusionAdapter{}.summarize(*candidate.final_shape()).volume_mm3 ==
            Approx(240));
  }
}

TEST_CASE("Imported targets resolve locally and through shared nested noncommuting occurrences",
          "[geometry][step-import-assembly-target]") {
  Directory directory;
  const auto supplier = cylinder(directory);
  const auto document = part("source", supplier);
  auto project = project_with_occurrences(document);
  const auto json = checked(serialize_project_to_json(project));
  project = checked(deserialize_project_from_json(json));
  StepImportTopologyContext context(directory.root);
  const auto expected_point = [](Point3 p) { return Point3{8 + p.y, p.x - 11, -96 - p.z}; };
  const auto expected_vector = [](Vector3 v) { return Vector3{v.y, v.x, -v.z}; };
  for (const auto& [geometry, capability] :
       std::vector<std::pair<ImportedGeometryClass, ImportedTopologyCapability>>{
           {ImportedGeometryClass::Plane, ImportedTopologyCapability::Plane},
           {ImportedGeometryClass::Cylinder, ImportedTopologyCapability::Cylinder},
           {ImportedGeometryClass::Line, ImportedTopologyCapability::Line},
           {ImportedGeometryClass::Circle, ImportedTopologyCapability::Circle},
           {ImportedGeometryClass::Point, ImportedTopologyCapability::Point}}) {
    const auto imported = reference(supplier.catalog, geometry, capability);
    const auto current = checked(context.resolve(document, imported));
    REQUIRE(current.descriptor);
    const auto& descriptor = *current.descriptor;
    const auto spelling = checked(make_imported_topology_target_spelling(imported));
    const auto local =
        checked(AssemblyConstraintTarget::create(ComponentInstanceId("local"), spelling));
    CHECK_FALSE(AssemblyConstraintTargetResolver{}.resolve_geometric(project, local));
    auto resolved =
        checked(AssemblyConstraintTargetResolver{&context}.resolve_geometric(project, local));
    CHECK(resolved.coordinate_space == AssemblyGeometricTargetCoordinateSpace::ComponentLocal);
    CHECK_FALSE(resolved.source_metadata.source_feature);
    REQUIRE(resolved.source_metadata.imported_topology);
    CHECK(*resolved.source_metadata.imported_topology == imported);
    REQUIRE(validate_resolved_geometric_target(resolved));
    const auto target = checked(AssemblyHierarchyConstraintTarget::create(
        {SubassemblyInstanceId("outer"), SubassemblyInstanceId("inner")},
        ComponentInstanceId("nested"), spelling));
    CHECK_FALSE(AssemblyHierarchyConstraintTargetResolver{}.resolve_geometric(project, target));
    const auto root = checked(
        AssemblyHierarchyConstraintTargetResolver{&context}.resolve_geometric(project, target));
    REQUIRE(root.transforms_inner_to_outer.size() == 3);
    CHECK(root.coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly);
    CHECK(root.source_metadata.imported_topology == resolved.source_metadata.imported_topology);
    if (geometry == ImportedGeometryClass::Plane) {
      const auto actual = checked(project_plane(root));
      point_is(actual.origin, expected_point(descriptor.origin));
      vector_is(actual.normal, expected_vector(descriptor.direction));
      auto wrong = resolved;
      auto mismatch = checked(ImportedTopologyReference::create(
          imported.catalog(), imported.entry(), ImportedTopologyCapability::Cylinder));
      wrong.source_metadata.imported_topology = mismatch;
      std::get<AssemblyLocalGeometricTargetEndpointIdentity>(wrong.endpoint).semantic_reference =
          checked(make_imported_topology_target_spelling(mismatch));
      CHECK_FALSE(validate_resolved_geometric_target(wrong));
      CHECK_FALSE(project_plane(wrong));
    } else if (geometry == ImportedGeometryClass::Cylinder) {
      const auto actual = checked(project_cylinder(root));
      point_is(actual.axis_origin, expected_point(descriptor.origin));
      vector_is(actual.axis_direction, expected_vector(descriptor.direction));
      CHECK(actual.radius_mm == Approx(5));
      REQUIRE(project_axis(root));
    } else if (geometry == ImportedGeometryClass::Circle) {
      const auto actual = checked(project_circle(root));
      point_is(actual.center, expected_point(descriptor.origin));
      vector_is(actual.normal, expected_vector(descriptor.direction));
      point_is(checked(project_point(root)).point, actual.center);
      REQUIRE(project_axis(root));
    } else if (geometry == ImportedGeometryClass::Line) {
      const auto actual = checked(project_line(root));
      point_is(actual.origin, expected_point(descriptor.origin));
      vector_is(actual.direction, expected_vector(descriptor.direction));
      CHECK_FALSE(project_axis(root));
    } else {
      point_is(checked(project_point(root)).point, expected_point(descriptor.origin));
    }
  }
  CHECK(checked(serialize_project_to_json(project)) == json);
  const auto original = bytes(directory.root / "supplier.step");
  write(directory.root / "supplier.step", original + "\n/* changed */\n");
  const auto imported =
      reference(supplier.catalog, ImportedGeometryClass::Point, ImportedTopologyCapability::Point);
  const auto target = checked(AssemblyHierarchyConstraintTarget::create(
      {SubassemblyInstanceId("outer"), SubassemblyInstanceId("inner")},
      ComponentInstanceId("nested"), checked(make_imported_topology_target_spelling(imported))));
  CHECK_FALSE(
      AssemblyHierarchyConstraintTargetResolver{&context}.resolve_geometric(project, target));
  CHECK(checked(serialize_project_to_json(project)) == json);
}
