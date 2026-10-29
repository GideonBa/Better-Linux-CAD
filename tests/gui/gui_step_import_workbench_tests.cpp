#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/reference_part_geometry.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/step_editable_commands.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/gui/gui_analysis_export_workbench.hpp"
#include "blcad/gui/gui_assembly_workbench.hpp"
#include "blcad/gui/gui_document_browser.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"
#include "blcad/gui/gui_feature_edit.hpp"
#include "gui_step_structured_import_fixture.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <nlohmann/json.hpp>

using namespace blcad;
using namespace blcad::gui;
namespace {
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
void write(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::WriteOnly));
  REQUIRE(file.write(bytes) == bytes.size());
}
QByteArray bytes(const QString& path) {
  QFile file(path);
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
std::string snapshot(const GuiDocumentSession& session) {
  return session.part_document()
             ? checked(serialize_part_document_to_json(*session.part_document()))
             : checked(serialize_project_to_json(*session.project()));
}
void create(GuiDocumentSession& session, bool project) {
  if (project)
    REQUIRE(session.create_project(DocumentId("project.source"), "Source project"));
  else
    REQUIRE(session.create_part(DocumentId("part.source"), "Source part"));
}
QString filename(bool project) {
  return project ? "model.blcad.project.json" : "model.blcad.json";
}
StepImportSource register_source(GuiDocumentSession& session, const QTemporaryDir& directory,
                                 const char* path = "source.step") {
  write(directory.filePath(path), "ISO-10303-21;\nsource A\nEND-ISO-10303-21;\n");
  const GuiStepImportWorkbench workbench;
  auto preview =
      checked(workbench.prepare_register(session, "Plate", path, StepImportMode::Reference));
  REQUIRE(workbench.apply(session, preview));
  return preview.source;
}
void write_geometry(const QString& path, double width = 20) {
  auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(width)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(5))));
  REQUIRE(geometry::StepExporter{}.write_step(shape, path.toStdString()));
}
void write_product_geometry(const QString& path,
                            const std::vector<std::pair<double, double>>& sizes) {
  auto part = checked(PartDocument::create(DocumentId("supplier.products"), "Supplier products"));
  REQUIRE(part.add_datum_plane(checked(DatumPlane::xy())));
  for (std::size_t i = 0; i < sizes.size(); ++i) {
    const auto suffix = std::to_string(i);
    for (const auto& [name, length] : std::vector<std::pair<std::string, double>>{
             {"width.", sizes[i].first}, {"height.", 10}, {"depth.", sizes[i].second}})
      REQUIRE(part.add_parameter(checked(Parameter::create_length(
          ParameterId(name + suffix), name + suffix, checked(Quantity::length_mm(length))))));
    const BodyId body("body." + suffix);
    REQUIRE(part.add_body(checked(Body::create(body, "Product " + suffix, BodyKind::Solid))));
    auto sketch = checked(Sketch::create(SketchId("sketch." + suffix), "Profile " + suffix,
                                         DatumPlaneId("datum.xy")));
    REQUIRE(sketch.add_profile(checked(RectangleProfile::create(ProfileId("profile." + suffix),
                                                                ParameterId("width." + suffix),
                                                                ParameterId("height." + suffix)))));
    REQUIRE(part.add_sketch(sketch));
    auto feature = checked(Feature::create_additive_extrude(FeatureId("feature." + suffix),
                                                            "Extrusion " + suffix, sketch.id(),
                                                            ParameterId("depth." + suffix)));
    feature = checked(feature.with_body_result_context(checked(
        FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt, body))));
    REQUIRE(part.add_feature(feature));
  }
  auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("supplier.products")));
  REQUIRE(geometry::GeometryRecomputeExecutor{}.execute_document(part, cache));
  const auto exported =
      checked(geometry::StepExporter{}.write_part_step(part, cache, path.toStdString()));
  REQUIRE(exported.exported_body_count == sizes.size());
}
std::size_t product_index(const geometry::StepImportGeometry& imported, double width,
                          double depth) {
  for (std::size_t index = 0; index < imported.products.size(); ++index) {
    const auto& product = imported.products[index];
    if (!product.is_assembly && product.bodies.size() == 1) {
      const auto& bounds = product.bodies.front().bounds_mm;
      if (std::abs(bounds[3] - bounds[0] - width) < 1e-6 &&
          std::abs(bounds[5] - bounds[2] - depth) < 1e-6)
        return index;
    }
  }
  FAIL("Expected supplier product dimensions were not transferred");
  return 0;
}
} // namespace

TEST_CASE("Structured STEP GUI preparation matches shared commands and saves a new Project without "
          "changing its registry",
          "[gui][step-structured-import][step-import-workbench]") {
  const bool registry_project = GENERATE(false, true);
  const auto mode = GENERATE(StepImportMode::Reference, StepImportMode::EditableBody);
  CAPTURE(registry_project, static_cast<int>(mode));
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  structured_test::write_supplier(directory.filePath("nested.step").toStdString());
  GuiDocumentSession session;
  create(session, registry_project);
  REQUIRE(session.save_as(directory.filePath(filename(registry_project)).toStdString()));
  const GuiStepImportWorkbench workbench;
  auto registration =
      checked(workbench.prepare_register(session, "Nested supplier", "nested.step", mode));
  REQUIRE(workbench.apply(session, registration));
  REQUIRE(session.dirty());
  const auto original = snapshot(session);
  const auto history = session.undo_label();
  const auto original_path = session.file_path();
  const auto original_file = bytes(QString::fromStdString(original_path.string()));
  const auto asset = bytes(directory.filePath("nested.step"));
  GuiStructuredStepImportRequest request{registration.source.id(), DocumentId("project.structured"),
                                         "Imported supplier assembly"};
  const auto preview = checked(workbench.prepare_structured_project(session, request));
  const auto direct = checked(geometry::StepStructuredImportCommands{}.create_project(
      registration.source, request.new_project, request.name, directory.path().toStdString()));
  const auto imported = checked(
      geometry::StepImportReader{}.read(registration.source, directory.path().toStdString()));
  CHECK(checked(serialize_project_to_json(preview.candidate.project)) ==
        checked(serialize_project_to_json(direct.project)));
  REQUIRE(preview.candidate.project.part_document_count() == 1);
  const auto foreign_part = std::find_if(imported.products.begin(), imported.products.end(),
                                         [](const auto& product) { return !product.is_assembly; });
  REQUIRE(foreign_part != imported.products.end());
  CHECK(preview.candidate.project.part_documents().front().name() == foreign_part->name);
  CHECK(preview.candidate.project.part_documents().front().is_reference_part() ==
        (mode == StepImportMode::Reference));
  structured_test::check_source_structure(preview.candidate.project, imported);
  CHECK(checked(AssemblyLeafOccurrenceResolver{}.resolve(preview.candidate.project)).size() == 5);
  CHECK(preview.preview_shapes.size() == 5);
  CHECK(snapshot(session) == original);
  CHECK(session.undo_label() == history);
  CHECK(session.dirty());
  const auto destination = directory.filePath("new.blcad.project.json");
  REQUIRE(workbench.save_structured_project_copy(session, preview, destination.toStdString()));
  CHECK(snapshot(session) == original);
  CHECK(session.file_path() == original_path);
  CHECK(session.undo_label() == history);
  CHECK(session.dirty());
  CHECK(bytes(QString::fromStdString(original_path.string())) == original_file);
  GuiDocumentSession opened;
  REQUIRE(opened.open_file(destination.toStdString()));
  REQUIRE(opened.project());
  CHECK(snapshot(opened) == checked(serialize_project_to_json(direct.project)));
  CHECK(bytes(directory.filePath("nested.step")) == asset);
}

TEST_CASE("Structured STEP copy rejects forged candidates and protected or existing destinations "
          "atomically",
          "[gui][step-structured-import][step-import-workbench]") {
  QTemporaryDir directory, elsewhere;
  REQUIRE(directory.isValid());
  REQUIRE(elsewhere.isValid());
  structured_test::write_supplier(directory.filePath("nested.step").toStdString());
  GuiDocumentSession session;
  create(session, false);
  REQUIRE(session.save_as(directory.filePath(filename(false)).toStdString()));
  const GuiStepImportWorkbench workbench;
  const auto registration = checked(workbench.prepare_register(
      session, "Nested supplier", "nested.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registration));
  REQUIRE(session.save());
  const auto original = snapshot(session);
  const auto history = session.undo_label();
  const auto asset = bytes(directory.filePath("nested.step"));
  const auto preview = checked(workbench.prepare_structured_project(
      session, {registration.source.id(), DocumentId("project.structured"), "Imported assembly"}));
  auto forged = preview;
  REQUIRE(forged.candidate.project.add_part_document(
      checked(PartDocument::create(DocumentId("part.injected"), "Injected"))));
  const auto output = directory.filePath("new.blcad.project.json");
  CHECK_FALSE(workbench.save_structured_project_copy(session, forged, output.toStdString()));
  CHECK_FALSE(QFile::exists(output));
  CHECK_FALSE(workbench.save_structured_project_copy(session, preview, session.file_path()));
  CHECK_FALSE(workbench.save_structured_project_copy(
      session, preview, directory.filePath("nested.step").toStdString()));
  std::error_code error;
  const auto alias = directory.filePath("source-alias.blcad.project.json");
  std::filesystem::create_hard_link(directory.filePath("nested.step").toStdString(),
                                    alias.toStdString(), error);
  REQUIRE_FALSE(error);
  CHECK_FALSE(workbench.save_structured_project_copy(session, preview, alias.toStdString()));
  write(output, "existing user project");
  CHECK_FALSE(workbench.save_structured_project_copy(session, preview, output.toStdString()));
  CHECK(bytes(output) == "existing user project");
  CHECK_FALSE(workbench.save_structured_project_copy(
      session, preview, elsewhere.filePath("new.blcad.project.json").toStdString()));
  CHECK(QDir(directory.path())
            .entryList({".blcad-structured-project-*"}, QDir::Files | QDir::Hidden)
            .isEmpty());
  CHECK(snapshot(session) == original);
  CHECK(session.undo_label() == history);
  CHECK_FALSE(session.dirty());
  CHECK(bytes(directory.filePath("nested.step")) == asset);
}

TEST_CASE(
    "Structured STEP copy validates session path intent and fresh source bytes before publication",
    "[gui][step-structured-import][step-import-workbench]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  structured_test::write_supplier(directory.filePath("nested.step").toStdString());
  GuiDocumentSession session;
  create(session, true);
  REQUIRE(session.save_as(directory.filePath(filename(true)).toStdString()));
  const GuiStepImportWorkbench workbench;
  const auto registration = checked(workbench.prepare_register(
      session, "Nested supplier", "nested.step", StepImportMode::EditableBody));
  REQUIRE(workbench.apply(session, registration));
  REQUIRE(session.save());
  const GuiStructuredStepImportRequest request{
      registration.source.id(), DocumentId("project.structured"), "Imported assembly"};
  auto preview = checked(workbench.prepare_structured_project(session, request));
  REQUIRE(session.commit_project_transaction("Registry edit", [](Project& project) {
    return project.add_part_document(
        checked(PartDocument::create(DocumentId("part.other"), "Other")));
  }));
  const auto edited = snapshot(session);
  const auto history = session.undo_label();
  const auto output = directory.filePath("new.blcad.project.json");
  CHECK_FALSE(workbench.save_structured_project_copy(session, preview, output.toStdString()));
  CHECK_FALSE(QFile::exists(output));
  preview = checked(workbench.prepare_structured_project(session, request));
  REQUIRE(session.save_as(directory.filePath("registry-renamed.blcad.project.json").toStdString()));
  CHECK_FALSE(workbench.validate_structured_project(session, preview));
  preview = checked(workbench.prepare_structured_project(session, request));
  const auto changed =
      bytes(directory.filePath("nested.step")) + QByteArray("\n/* supplier revision */\n");
  write(directory.filePath("nested.step"), changed);
  CHECK_FALSE(workbench.save_structured_project_copy(session, preview, output.toStdString()));
  CHECK_FALSE(QFile::exists(output));
  CHECK_FALSE(workbench.prepare_structured_project(session, request));
  CHECK(snapshot(session) == edited);
  CHECK(session.undo_label() == history);
  CHECK(bytes(directory.filePath("nested.step")) == changed);
  CHECK(QDir(directory.path())
            .entryList({".blcad-structured-project-*"}, QDir::Files | QDir::Hidden)
            .isEmpty());
}

TEST_CASE("Direct EditableBody source commands create standalone and Project imported base intent",
          "[gui][step-editable-parts][step-import-workbench]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto document = directory.filePath(filename(project));
  const auto asset = directory.filePath("editable.step");
  write_geometry(asset);
  const auto source_bytes = bytes(asset);
  REQUIRE(session.save_as(document.toStdString()));
  const GuiStepImportWorkbench workbench;
  const auto source = checked(workbench.prepare_register(
      session, "Editable supplier", "editable.step", StepImportMode::EditableBody));
  REQUIRE(workbench.apply(session, source));
  const auto topology = checked(workbench.prepare_topology(session, source.source.id(), 0));
  REQUIRE(workbench.apply_topology(session, topology));
  const auto before = snapshot(session);
  GuiStepReferenceRequest request;
  request.catalog = topology.catalog.id();
  request.new_part = DocumentId("editable.direct");
  request.name = "Direct editable";
  if (project) {
    request.target_assembly = session.project()->assembly().id();
    request.instance = ComponentInstanceId("editable.instance");
  }
  const auto prepared = checked(workbench.prepare_reference(session, request));
  CHECK(snapshot(session) == before);
  REQUIRE(prepared.preview_shapes.size() == 1);
  CHECK(
      geometry::RectangleExtrusionAdapter{}.summarize(prepared.preview_shapes.front()).volume_mm3 ==
      Catch::Approx(1000).margin(1e-5));
  if (project) {
    REQUIRE(workbench.apply_reference(session, prepared));
    REQUIRE(session.project()->part_document_count() == 1);
    const auto* part = session.project()->find_part_document(request.new_part);
    REQUIRE(part);
    CHECK_FALSE(part->is_reference_part());
    REQUIRE(part->imported_body_features().size() == 1);
    CHECK(part->imported_body_features().front().id() == request.base_feature);
    CHECK(session.project()->assembly().component_instances().front().referenced_part_document() ==
          request.new_part);
    const auto accepted = snapshot(session);
    REQUIRE(session.undo());
    CHECK(snapshot(session) == before);
    REQUIRE(session.redo());
    CHECK(snapshot(session) == accepted);
  } else {
    const auto destination = directory.filePath("direct.blcad.json");
    REQUIRE(workbench.save_editable_copy(session, prepared, destination.toStdString()));
    CHECK(snapshot(session) == before);
    GuiDocumentSession editable;
    REQUIRE(editable.open_file(destination.toStdString()));
    REQUIRE(editable.part_document());
    CHECK_FALSE(editable.part_document()->is_reference_part());
    REQUIRE(editable.part_document()->imported_body_features().size() == 1);
    CHECK(editable.part_shape_cache()->feature_shape_count() == 1);
    CHECK(editable.part_shape_cache()->body_shape_count() == 1);
    GuiFeatureEditController editor;
    const auto rejected = editor.begin(editable, request.base_feature);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().message().find("immutable") != std::string::npos);
    CHECK_FALSE(workbench.save_editable_copy(session, prepared, destination.toStdString()));
  }
  CHECK(bytes(asset) == source_bytes);
}

TEST_CASE(
    "Editable refresh binds candidate intent and source evidence and preserves atomic history",
    "[gui][step-editable-parts][step-import-workbench]") {
  GuiDocumentSession registered;
  create(registered, false);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto asset = directory.filePath("editable.step");
  write_geometry(asset);
  REQUIRE(registered.save_as(directory.filePath("registered.blcad.json").toStdString()));
  const GuiStepImportWorkbench workbench;
  const auto source = checked(workbench.prepare_register(registered, "Editable", "editable.step",
                                                         StepImportMode::EditableBody));
  REQUIRE(workbench.apply(registered, source));
  const auto topology = checked(workbench.prepare_topology(registered, source.source.id(), 0));
  REQUIRE(workbench.apply_topology(registered, topology));
  GuiStepReferenceRequest insert;
  insert.catalog = topology.catalog.id();
  insert.new_part = DocumentId("editable.actual");
  insert.name = "Editable actual";
  const auto preview = checked(workbench.prepare_reference(registered, insert));
  const auto file = directory.filePath("editable.blcad.json");
  REQUIRE(workbench.save_editable_copy(registered, preview, file.toStdString()));
  GuiDocumentSession session;
  REQUIRE(session.open_file(file.toStdString()));
  const auto before = snapshot(session);
  const auto history = std::string(session.undo_label());
  GuiStepReferenceRequest refresh;
  refresh.operation = GuiStepReferenceOperation::Refresh;
  refresh.refresh_source = source.source.id();
  auto candidate = checked(workbench.prepare_reference(session, refresh));
  auto forged = candidate;
  auto& forged_part = std::get<geometry::StepReferencePartCandidate>(forged.candidate).part;
  REQUIRE(forged_part.add_parameter(checked(
      Parameter::create_length(ParameterId("forged"), "Forged", checked(Quantity::length_mm(1))))));
  CHECK_FALSE(workbench.apply_reference(session, forged));
  CHECK(snapshot(session) == before);
  auto revised = bytes(asset);
  revised.insert(revised.indexOf("ENDSEC;"), "/* accepted revision B */\n");
  write(asset, revised);
  CHECK_FALSE(workbench.apply_reference(session, candidate));
  CHECK(snapshot(session) == before);
  CHECK(session.undo_label() == history);
  CHECK_FALSE(session.dirty());
  candidate = checked(workbench.prepare_reference(session, refresh));
  REQUIRE(workbench.apply_reference(session, candidate));
  const auto accepted = snapshot(session);
  CHECK(accepted != before);
  CHECK(session.part_document()->imported_body_features().front().id() == insert.base_feature);
  CHECK(session.part_shape_cache()->feature_shape_count() == 1);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == accepted);
  REQUIRE(session.save());
  REQUIRE(session.open_file(file.toStdString()));
  CHECK(snapshot(session) == accepted);
  CHECK(bytes(asset) == revised);
}

TEST_CASE(
    "Current-digest imported face loss retains Part intent and history but rejects malformed roles",
    "[gui][step-editable-parts][step-import-workbench]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto root = std::filesystem::path(directory.path().toStdString());
  const auto asset = directory.filePath("supplier.step");
  write_geometry(asset);
  const auto inspected = checked(inspect_step_import_asset(root, "supplier.step"));
  auto source = checked(
      StepImportSource::create(StepImportSourceId("supplier"), "Supplier", "supplier.step",
                               *inspected.observed_sha256_digest(), StepImportMode::EditableBody));
  const auto imported = checked(geometry::StepImportReader{}.read(source, root));
  const auto catalog = checked(geometry::StepImportTopology{}.create_catalog(
      ImportedTopologyCatalogId("catalog.supplier"), imported, 0));
  auto part = checked(geometry::StepEditableCommands{}.create_definition(
      source, catalog, DocumentId("part.recovery"), "Imported face recovery",
      FeatureId("import.base"), root));
  const auto body = part.bodies().front().id();
  auto cache = checked(geometry::ShapeCache::create(ShapeCacheId("recovery.fixture")));
  geometry::StepImportTopologyContext context(root);
  REQUIRE(geometry::GeometryRecomputeExecutor{&context}.execute_document(part, cache));
  const auto targets =
      checked(geometry::ResultFaceSupportGeometry::selectable_faces(part, cache, body, &context));
  const auto top = std::find_if(targets.begin(), targets.end(), [](const auto& target) {
    return std::abs(target.anchor.z - 5) < 1e-8;
  });
  REQUIRE(top != targets.end());
  const auto plane = checked(ResultFaceWorkplane::create(DatumPlaneId("support.imported.top"),
                                                         "Imported top", top->reference));
  for (const auto& [id, value] :
       std::vector<std::pair<std::string, double>>{{"width", 4}, {"height", 4}, {"depth", 3}})
    REQUIRE(part.add_parameter(checked(
        Parameter::create_length(ParameterId(id), id, checked(Quantity::length_mm(value))))));
  auto sketch = checked(Sketch::create(SketchId("boss.sketch"), "Boss", plane.id()));
  REQUIRE(sketch.add_profile(checked(RectangleProfile::create(
      ProfileId("boss.profile"), ParameterId("width"), ParameterId("height")))));
  REQUIRE(part.add_face_supported_sketch(plane, sketch));
  auto boss = checked(Feature::create_additive_extrude(FeatureId("boss"), "Boss", sketch.id(),
                                                       ParameterId("depth")));
  boss = checked(boss.with_body_result_context(checked(
      FeatureBodyResultContext::create(FeatureBodyOperationMode::Join, body, std::nullopt))));
  REQUIRE(part.add_feature(boss));
  const auto document = directory.filePath("recovery.blcad.json");
  REQUIRE(write_part_document_json_file(part, document.toStdString()));
  GuiDocumentSession session;
  REQUIRE(session.open_file(document.toStdString()));
  REQUIRE(session.part_shape_cache());
  auto revised = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
      checked(Quantity::length_mm(20)), checked(Quantity::length_mm(10)),
      checked(Quantity::length_mm(8))));
  REQUIRE(geometry::StepExporter{}.write_step(revised, asset.toStdString()));
  const auto report = checked(inspect_step_import_source(source, root));
  REQUIRE(refresh_step_import_source(part, root, report));
  const auto intended = checked(serialize_part_document_to_json(part));
  REQUIRE(write_part_document_json_file(part, document.toStdString()));
  geometry::StepImportTopologyContext recovery_context(root);
  const auto recovery = checked(recovery_context.recover(part, catalog.id()));
  const auto body_entry =
      part.imported_body_features().front().definition().outputs().front().imported_body;
  REQUIRE(recovery->find(body_entry));
  CHECK(recovery->find(body_entry)->status == ImportedTopologyRecoveryStatus::Resolved);
  const auto face = checked(parse_imported_face_origin_role(top->reference.origins().front().role));
  REQUIRE(recovery->find(face.entry()));
  CHECK(recovery->find(face.entry())->status == ImportedTopologyRecoveryStatus::Lost);
  REQUIRE(session.open_file(document.toStdString()));
  CHECK(snapshot(session) == intended);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK_FALSE(session.part_shape_cache());
  CHECK_FALSE(session.reference_repair_mode());
  CHECK_FALSE(session.dirty());
  REQUIRE_FALSE(session.diagnostics().empty());
  // An explicit headless recovery transaction removes the dependent authored
  // workflow, preserving the exact imported source/catalog/base/output intent.
  const auto repaired = checked(PartDocument::create_editable_import(
      part.id(), part.name(), part.step_import_sources().front(), catalog,
      part.imported_body_features().front()));
  REQUIRE(session.commit_part_transaction("Remove lost face workflow", [&](PartDocument& target) {
    target = repaired;
    return Result<std::size_t>::success(1);
  }));
  const auto recovered = snapshot(session);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Fresh);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == intended);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK_FALSE(session.part_shape_cache());
  REQUIRE(session.redo());
  CHECK(snapshot(session) == recovered);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Fresh);
  auto malformed = nlohmann::json::parse(intended);
  malformed["result_face_workplanes"][0]["reference"]["origins"][0]["role"] =
      "imported-face/malformed";
  const auto invalid_file = directory.filePath("malformed.blcad.json");
  const auto invalid_json = malformed.dump();
  write(invalid_file, QByteArray::fromStdString(invalid_json));
  CHECK_FALSE(session.open_file(invalid_file.toStdString(), true));
  CHECK(snapshot(session) == recovered);
}

TEST_CASE(
    "STEP geometry inspection provides posed bodies units and diagnostics without model intent",
    "[gui][step-import-workbench][step-import-inspection]") {
  const bool project = GENERATE(false, true);
  const auto mode = GENERATE(StepImportMode::Reference, StepImportMode::EditableBody);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const GuiStepImportWorkbench workbench;
  write_geometry(directory.filePath("source.step"));
  CHECK_FALSE(workbench.inspect_geometry(session, "Supplier", "source.step", mode));
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto before = snapshot(session);
  const auto asset = bytes(directory.filePath("source.step"));
  const auto undo = session.can_undo();
  const auto redo = session.can_redo();
  const auto preview =
      checked(workbench.inspect_geometry(session, "Supplier", "source.step", mode));
  CHECK_FALSE(preview.registered_source);
  CHECK(preview.source.mode() == mode);
  CHECK(preview.report.state() == StepImportFreshness::Available);
  REQUIRE(preview.geometry.units.size() == 1);
  CHECK(preview.geometry.units.front().length_to_mm == Catch::Approx(1));
  REQUIRE_FALSE(preview.geometry.products.empty());
  CHECK(preview.geometry.products.front().source_kind == geometry::StepImportedShapeKind::Solid);
  CHECK(preview.geometry.body_count() == 1);
  REQUIRE(preview.preview_shapes.size() == 1);
  REQUIRE_FALSE(preview.geometry.diagnostics.empty());
  CHECK(
      geometry::RectangleExtrusionAdapter{}.summarize(preview.preview_shapes.front()).volume_mm3 ==
      Catch::Approx(1000).margin(1e-5));
  REQUIRE_FALSE(preview.geometry.products.front().bodies.empty());
  const auto& body = preview.geometry.products.front().bodies.front();
  CHECK(body.kind == BodyKind::Solid);
  CHECK(body.volume_mm3 == Catch::Approx(1000).margin(1e-5));
  CHECK(body.bounds_mm[3] - body.bounds_mm[0] == Catch::Approx(20).margin(1e-5));
  CHECK(body.bounds_mm[4] - body.bounds_mm[1] == Catch::Approx(10).margin(1e-5));
  CHECK(body.bounds_mm[5] - body.bounds_mm[2] == Catch::Approx(5).margin(1e-5));
  CHECK(checked(workbench.validate_inspection(session, preview)) == 1);
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.dirty());
  CHECK(session.can_undo() == undo);
  CHECK(session.can_redo() == redo);
  CHECK(bytes(directory.filePath("source.step")) == asset);
  REQUIRE(session.save());
  CHECK(workbench.validate_inspection(session, preview));
  REQUIRE(session.save_as(directory.filePath("renamed.blcad.json").toStdString()));
  CHECK_FALSE(workbench.validate_inspection(session, preview));
  CHECK(snapshot(session) == before);
}

TEST_CASE(
    "STEP geometry inspection rejects changed registered bytes until explicit checksum refresh",
    "[gui][step-import-workbench][step-import-inspection][step-import-freshness]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("model.blcad.json").toStdString()));
  const auto asset = directory.filePath("source.step");
  write_geometry(asset);
  const GuiStepImportWorkbench workbench;
  const auto registration = checked(
      workbench.prepare_register(session, "Supplier", "source.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registration));
  REQUIRE(session.save());
  const auto preview = checked(workbench.inspect_geometry(session, registration.source.id()));
  CHECK(preview.registered_source);
  const auto before = snapshot(session);
  const auto undo_label = std::string(session.undo_label());
  write_geometry(asset, 40);
  const auto changed_bytes = bytes(asset);
  CHECK_FALSE(workbench.validate_inspection(session, preview));
  CHECK_FALSE(workbench.inspect_geometry(session, registration.source.id()));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.dirty());
  CHECK(session.undo_label() == undo_label);
  const auto refresh = checked(workbench.prepare_refresh(session, registration.source.id()));
  CHECK(snapshot(session) == before);
  REQUIRE(workbench.apply(session, refresh));
  const auto updated = checked(workbench.inspect_geometry(session, registration.source.id()));
  CHECK(
      geometry::RectangleExtrusionAdapter{}.summarize(updated.preview_shapes.front()).volume_mm3 ==
      Catch::Approx(2000).margin(1e-5));
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK_FALSE(workbench.validate_inspection(session, updated));
  REQUIRE(session.can_redo());
  CHECK_FALSE(workbench.inspect_geometry(session, registration.source.id()));
  REQUIRE(session.can_redo());
  CHECK(bytes(asset) == changed_bytes);
  REQUIRE(QFile::remove(asset));
  CHECK_FALSE(workbench.inspect_geometry(session, registration.source.id()));
  CHECK(snapshot(session) == before);
}

TEST_CASE(
    "Malformed STEP geometry leaves opaque source registration and inspection history separate",
    "[gui][step-import-workbench][step-import-inspection]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("model.blcad.json").toStdString()));
  write(directory.filePath("source.step"), "ISO-10303-21;\nnot transferable\nEND-ISO-10303-21;\n");
  const GuiStepImportWorkbench workbench;
  const auto before = snapshot(session);
  CHECK_FALSE(
      workbench.inspect_geometry(session, "Supplier", "source.step", StepImportMode::Reference));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  const auto registry = checked(
      workbench.prepare_register(session, "Supplier", "source.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registry));
  const auto registered = snapshot(session);
  CHECK_FALSE(workbench.inspect_geometry(session, registry.source.id()));
  CHECK(snapshot(session) == registered);
  CHECK(session.undo_label() == "Register STEP source");
}

TEST_CASE("Imported topology capture is an explicit atomic catalog transaction shared by Part and "
          "Project",
          "[gui][step-import-workbench][step-import-topology-workbench]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  write_geometry(directory.filePath("source.step"));
  const auto asset = bytes(directory.filePath("source.step"));
  const GuiStepImportWorkbench workbench;
  const auto registry = checked(
      workbench.prepare_register(session, "Supplier", "source.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registry));
  REQUIRE(session.save());
  const auto before = snapshot(session);
  const auto undo = std::string(session.undo_label());
  auto preview = checked(workbench.prepare_topology(session, registry.source.id(), 0));
  CHECK_FALSE(preview.accepted_catalog);
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.dirty());
  CHECK(session.undo_label() == undo);
  REQUIRE_FALSE(preview.catalog.entries().empty());
  for (const auto& entry : preview.catalog.entries()) {
    const auto* resolved = preview.recovery.find(entry.id);
    REQUIRE(resolved);
    CHECK(resolved->status == ImportedTopologyRecoveryStatus::Resolved);
    REQUIRE_FALSE(resolved->shape.empty());
    REQUIRE_FALSE(resolved->capabilities.empty());
    auto reference = checked(ImportedTopologyReference::create(preview.catalog.id(), entry.id,
                                                               resolved->capabilities.front()));
    CHECK(geometry::StepImportTopology{}.resolve(preview.recovery, reference));
    const auto spelling = checked(make_imported_topology_target_spelling(reference));
    CHECK(checked(parse_imported_topology_target_spelling(spelling)) == reference);
  }
  auto forged = preview;
  forged.captured_product_index = 9999;
  CHECK_FALSE(workbench.apply_topology(session, forged));
  CHECK(snapshot(session) == before);
  REQUIRE(workbench.apply_topology(session, preview));
  const auto accepted = snapshot(session);
  CHECK(accepted != before);
  CHECK(session.undo_label() == "Capture imported topology catalog");
  CHECK_FALSE(workbench.apply_topology(session, preview));
  CHECK_FALSE(workbench.prepare_topology(session, registry.source.id(), 0));
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == accepted);
  REQUIRE(session.save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  CHECK(snapshot(reopened) == accepted);
  const auto restored = checked(workbench.inspect_topology(reopened, preview.catalog.id()));
  CHECK(restored.accepted_catalog);
  CHECK(restored.catalog == preview.catalog);
  CHECK(checked(workbench.apply_topology(reopened, restored)) == 0);
  CHECK_FALSE(reopened.dirty());
  CHECK_FALSE(reopened.can_undo());
  const auto remove = [&](auto& document) {
    return document.remove_imported_topology_catalog(preview.catalog.id());
  };
  REQUIRE((project ? reopened.commit_project_transaction("Remove catalog", remove)
                   : reopened.commit_part_transaction("Remove catalog", remove)));
  REQUIRE(reopened.undo());
  REQUIRE(reopened.can_redo());
  const auto unchanged = checked(workbench.inspect_topology(reopened, preview.catalog.id()));
  CHECK(checked(workbench.apply_topology(reopened, unchanged)) == 0);
  CHECK(reopened.can_redo());
  CHECK(snapshot(reopened) == accepted);
  CHECK(bytes(directory.filePath("source.step")) == asset);
}

TEST_CASE("Imported topology Apply rejects an edited public preview targeting an already captured "
          "product",
          "[gui][step-import-workbench][step-import-topology-workbench][step-import-topology-"
          "duplicate]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("products.blcad.json").toStdString()));
  const auto path = directory.filePath("products.step");
  write_product_geometry(path, {{20, 5}, {40, 5}});
  const auto asset = bytes(path);
  const GuiStepImportWorkbench workbench;
  const auto registry = checked(
      workbench.prepare_register(session, "Products", "products.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registry));
  const auto inspected = checked(workbench.inspect_geometry(session, registry.source.id()));
  const auto first = product_index(inspected.geometry, 20, 5);
  const auto second = product_index(inspected.geometry, 40, 5);
  REQUIRE(first != second);
  const auto captured = checked(workbench.prepare_topology(session, registry.source.id(), first));
  REQUIRE(workbench.apply_topology(session, captured));
  CHECK_FALSE(workbench.prepare_topology(session, registry.source.id(), first));
  auto edited = checked(workbench.prepare_topology(session, registry.source.id(), second));
  const auto before = snapshot(session);
  const auto undo = std::string(session.undo_label());
  edited.catalog = checked(geometry::StepImportTopology{}.create_catalog(
      edited.catalog.id(), edited.inspection.geometry, first));
  edited.recovery =
      checked(geometry::StepImportTopology{}.recover(edited.catalog, edited.inspection.geometry));
  edited.captured_product_index = first;
  REQUIRE(workbench.validate_topology(session, edited));
  CHECK_FALSE(workbench.apply_topology(session, edited));
  CHECK(snapshot(session) == before);
  CHECK(session.undo_label() == undo);
  CHECK(session.part_document()->imported_topology_catalogs().size() == 1);
  CHECK(bytes(path) == asset);
}

TEST_CASE("Ambiguous captured products do not block a different product excluded by exact product "
          "evidence",
          "[gui][step-import-workbench][step-import-topology-workbench][step-import-topology-"
          "duplicate]") {
  const bool shares_trimmed_face = GENERATE(false, true);
  CAPTURE(shares_trimmed_face);
  const double width = shares_trimmed_face ? 20 : 40;
  const double depth = shares_trimmed_face ? 10 : 5;
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("products.blcad.json").toStdString()));
  const auto path = directory.filePath("products.step");
  write_product_geometry(path, {{20, 5}, {20, 5}, {width, depth}});
  const auto asset = bytes(path);
  const GuiStepImportWorkbench workbench;
  const auto registry = checked(
      workbench.prepare_register(session, "Products", "products.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registry));
  const auto inspected = checked(workbench.inspect_geometry(session, registry.source.id()));
  REQUIRE(inspected.geometry.products.size() == 3);
  const auto repeated = product_index(inspected.geometry, 20, 5);
  const auto distinct = product_index(inspected.geometry, width, depth);
  const auto captured =
      checked(workbench.prepare_topology(session, registry.source.id(), repeated));
  bool ambiguous = false;
  for (const auto& entry : captured.recovery.entries)
    ambiguous = ambiguous || entry.status == ImportedTopologyRecoveryStatus::Ambiguous;
  REQUIRE(ambiguous);
  REQUIRE(workbench.apply_topology(session, captured));
  const auto before = snapshot(session);
  const auto undo = std::string(session.undo_label());
  const auto fresh = workbench.prepare_topology(session, registry.source.id(), distinct);
  INFO((fresh ? "prepared" : fresh.error().message()));
  REQUIRE(fresh);
  CHECK(snapshot(session) == before);
  CHECK(session.undo_label() == undo);
  REQUIRE(workbench.apply_topology(session, fresh.value()));
  CHECK(session.part_document()->imported_topology_catalogs().size() == 2);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(session.part_document()->imported_topology_catalogs().size() == 2);
  CHECK(bytes(path) == asset);
}

TEST_CASE(
    "Imported topology preview fails stale Apply and retains baseline IDs after source refresh",
    "[gui][step-import-workbench][step-import-topology-workbench]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath("model.blcad.json").toStdString()));
  const auto asset = directory.filePath("source.step");
  write_geometry(asset);
  const GuiStepImportWorkbench workbench;
  const auto registry = checked(
      workbench.prepare_register(session, "Supplier", "source.step", StepImportMode::EditableBody));
  REQUIRE(workbench.apply(session, registry));
  auto preview = checked(workbench.prepare_topology(session, registry.source.id(), 0));
  const auto before = snapshot(session);
  REQUIRE(session.save_as(directory.filePath("moved.blcad.json").toStdString()));
  CHECK_FALSE(workbench.apply_topology(session, preview));
  CHECK(snapshot(session) == before);
  preview = checked(workbench.prepare_topology(session, registry.source.id(), 0));
  REQUIRE(workbench.apply_topology(session, preview));
  const auto catalog = preview.catalog;
  const auto accepted = snapshot(session);
  write_geometry(asset, 40);
  CHECK_FALSE(workbench.inspect_topology(session, catalog.id()));
  CHECK_FALSE(workbench.validate_topology(session, preview));
  CHECK(snapshot(session) == accepted);
  const auto refresh = checked(workbench.prepare_refresh(session, registry.source.id()));
  REQUIRE(workbench.apply(session, refresh));
  const auto recovered = checked(workbench.inspect_topology(session, catalog.id()));
  CHECK(recovered.catalog == catalog);
  bool lost = false;
  for (const auto& entry : recovered.recovery.entries)
    if (entry.status != ImportedTopologyRecoveryStatus::Resolved) {
      lost = true;
      CHECK(entry.shape.empty());
      CHECK_FALSE(entry.descriptor);
    }
  CHECK(lost);
  const auto refreshed = snapshot(session);
  CHECK(checked(workbench.apply_topology(session, recovered)) == 0);
  CHECK(snapshot(session) == refreshed);
  CHECK(session.part_document()->find_imported_topology_catalog(catalog.id()) != nullptr);
}

TEST_CASE("STEP source tasks require Save and register exact portable intent with atomic history",
          "[gui][step-import-workbench][step-import-source]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const GuiStepImportWorkbench workbench;
  CHECK_FALSE(
      workbench.prepare_register(session, "Source", "source.step", StepImportMode::Reference));
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto before = snapshot(session);
  write(directory.filePath("source.step"), "source A");
  auto preview = checked(
      workbench.prepare_register(session, "Plate", "./source.step", StepImportMode::EditableBody));
  CHECK(preview.source.project_relative_asset_path() == "source.step");
  CHECK(preview.source.mode() == StepImportMode::EditableBody);
  CHECK(preview.report.state() == StepImportFreshness::Available);
  CHECK(snapshot(session) == before);
  CHECK_FALSE(session.can_undo());
  REQUIRE(session.save());
  REQUIRE(workbench.apply(session, preview));
  const auto after = snapshot(session);
  CHECK(after != before);
  CHECK(session.undo_label() == "Register STEP source");
  CHECK_FALSE(
      workbench.prepare_register(session, "Duplicate", "source.step", StepImportMode::Reference));
  auto statuses = checked(workbench.inspect_sources(session));
  REQUIRE(statuses.size() == 1);
  CHECK(statuses.front().report.state() == StepImportFreshness::Available);
  CHECK_FALSE(statuses.front().embedded_part);
  auto browser = GuiDocumentBrowser::build(session);
  const auto& owner_id = statuses.front().owner_document.value();
  const auto& source_id = preview.source.id().value();
  const auto id = std::string("step-source/") + (project ? "project/" : "part/") +
                  std::to_string(owner_id.size()) + ":" + owner_id + "/" +
                  std::to_string(source_id.size()) + ":" + source_id;
  const auto* node = browser.find(id);
  REQUIRE(node);
  CHECK(node->kind == GuiBrowserNodeKind::ExternalSource);
  CHECK(node->viewport_semantic_id.empty());
  for (const auto& property : node->properties)
    CHECK_FALSE(property.editable);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  REQUIRE(session.save());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  CHECK(snapshot(reopened) == after);
  auto unchanged = checked(workbench.prepare_refresh(reopened, preview.source.id()));
  CHECK(checked(workbench.apply(reopened, unchanged)) == 0);
  CHECK_FALSE(reopened.can_undo());
  const auto removal = checked(workbench.prepare_remove(reopened, preview.source.id()));
  REQUIRE(workbench.apply(reopened, removal));
  REQUIRE(checked(workbench.inspect_sources(reopened)).empty());
  REQUIRE(reopened.undo());
  REQUIRE(reopened.can_redo());
  const auto before_refresh = snapshot(reopened);
  unchanged = checked(workbench.prepare_refresh(reopened, preview.source.id()));
  CHECK(checked(workbench.apply(reopened, unchanged)) == 0);
  CHECK(snapshot(reopened) == before_refresh);
  REQUIRE(reopened.can_redo());
  REQUIRE(reopened.redo());
  CHECK(checked(workbench.inspect_sources(reopened)).empty());
  CHECK(bytes(directory.filePath("source.step")) == "source A");
}

TEST_CASE(
    "STEP source refresh rereads bytes rejects stale intent and accepts only inspected digest",
    "[gui][step-import-workbench][step-import-freshness]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto source = register_source(session, directory);
  const GuiStepImportWorkbench workbench;
  const auto before = snapshot(session);
  write(directory.filePath("source.step"), "source B");
  auto preview = checked(workbench.prepare_refresh(session, source.id()));
  CHECK(preview.report.state() == StepImportFreshness::Changed);
  CHECK(snapshot(session) == before);
  write(directory.filePath("source.step"), "source C");
  CHECK_FALSE(workbench.apply(session, preview));
  CHECK(snapshot(session) == before);
  preview = checked(workbench.prepare_refresh(session, source.id()));
  REQUIRE(workbench.apply(session, preview));
  CHECK(checked(workbench.inspect(session, source.id())).state() == StepImportFreshness::Available);
  const auto refreshed = snapshot(session);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK(checked(workbench.inspect(session, source.id())).state() == StepImportFreshness::Changed);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == refreshed);
  auto removal = checked(workbench.prepare_remove(session, source.id()));
  register_source(session, directory, "second.step");
  CHECK_FALSE(workbench.apply(session, removal));
  auto forged = checked(workbench.prepare_remove(session, source.id()));
  forged.source =
      checked(StepImportSource::create(source.id(), "Forged", "source.step",
                                       forged.source.sha256_digest(), StepImportMode::Reference));
  CHECK_FALSE(workbench.apply(session, forged));
  CHECK(bytes(directory.filePath("source.step")) == "source C");
}

TEST_CASE("Missing and unreadable STEP metadata stays openable for explicit recovery",
          "[gui][step-import-workbench][step-import-freshness]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir directory;
  REQUIRE(session.save_as(directory.filePath(filename(project)).toStdString()));
  const auto source = register_source(session, directory);
  REQUIRE(session.save());
  REQUIRE(QFile::remove(directory.filePath("source.step")));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(session.file_path()));
  const auto before = snapshot(reopened);
  const GuiStepImportWorkbench workbench;
  CHECK(reopened.derived_results_fresh());
  CHECK(checked(workbench.inspect(reopened, source.id())).state() == StepImportFreshness::Missing);
  REQUIRE_FALSE(reopened.diagnostics().empty());
  CHECK(reopened.diagnostics().back().severity == GuiDiagnosticSeverity::Warning);
  CHECK_FALSE(workbench.prepare_refresh(reopened, source.id()));
  REQUIRE(reopened.save());
  auto missing = checked(workbench.prepare_remove(reopened, source.id()));
  REQUIRE(QDir().mkdir(directory.filePath("source.step")));
  CHECK_FALSE(workbench.apply(reopened, missing));
  CHECK(snapshot(reopened) == before);
  CHECK(checked(workbench.inspect(reopened, source.id())).state() ==
        StepImportFreshness::Unreadable);
  auto unreadable = checked(workbench.prepare_remove(reopened, source.id()));
  REQUIRE(workbench.apply(reopened, unreadable));
  CHECK(checked(workbench.inspect_sources(reopened)).empty());
  REQUIRE(reopened.undo());
  CHECK(snapshot(reopened) == before);
  REQUIRE(QDir().rmdir(directory.filePath("source.step")));
  write(directory.filePath("source.step"), "Recovered source");
  auto refresh = checked(workbench.prepare_refresh(reopened, source.id()));
  REQUIRE(workbench.apply(reopened, refresh));
  CHECK(checked(workbench.inspect(reopened, source.id())).state() ==
        StepImportFreshness::Available);
}

TEST_CASE("Save As requires matching relocated assets and invalidates old source previews",
          "[gui][step-import-workbench][step-import-json]") {
  const bool project = GENERATE(false, true);
  GuiDocumentSession session;
  create(session, project);
  QTemporaryDir original, moved;
  REQUIRE(session.save_as(original.filePath(filename(project)).toStdString()));
  const auto source = register_source(session, original);
  REQUIRE(session.save());
  const auto old_path = session.file_path();
  const auto before = snapshot(session);
  const GuiStepImportWorkbench workbench;
  const auto preview = checked(workbench.prepare_remove(session, source.id()));
  const auto target = moved.filePath(filename(project));
  CHECK_FALSE(session.save_as(target.toStdString()));
  CHECK_FALSE(QFile::exists(target));
  CHECK(session.file_path() == old_path);
  write(moved.filePath("source.step"), "Wrong source");
  CHECK_FALSE(session.save_as(target.toStdString()));
  CHECK(snapshot(session) == before);
  write(moved.filePath("source.step"), bytes(original.filePath("source.step")));
  REQUIRE(session.save_as(target.toStdString()));
  CHECK(snapshot(session) == before);
  CHECK_FALSE(workbench.apply(session, preview));
  CHECK_FALSE(session.dirty());
  REQUIRE(QFile::remove(original.filePath("source.step")));
  CHECK(checked(workbench.inspect(session, source.id())).state() == StepImportFreshness::Available);
}

TEST_CASE("Document and STEP export writers preserve source assets and aliases",
          "[gui][step-import-workbench][step-import-source]") {
  GuiDocumentSession session;
  create(session, false);
  QTemporaryDir directory;
  const auto document = directory.filePath("model.blcad.json");
  REQUIRE(session.save_as(document.toStdString()));
  const GuiStepImportWorkbench workbench;
  CHECK_FALSE(
      workbench.prepare_register(session, "Self", "model.blcad.json", StepImportMode::Reference));
  register_source(session, directory);
  const auto content = bytes(directory.filePath("source.step"));
  std::filesystem::create_hard_link(directory.filePath("source.step").toStdString(),
                                    directory.filePath("alias.step").toStdString());
  for (const auto* name : {"source.step", "alias.step"}) {
    const auto path = directory.filePath(name).toStdString();
    CHECK_FALSE(session.save_as(path));
    CHECK_FALSE(GuiAnalysisExportWorkbench{}.preflight_step_export(
        session, {GuiStepExportMode::PartMultiBody, path}));
    CHECK(bytes(directory.filePath(name)) == content);
  }
  REQUIRE(QFile::remove(directory.filePath("source.step")));
  CHECK_FALSE(session.save_as(directory.filePath("source.step").toStdString()));
  CHECK_FALSE(QFile::exists(directory.filePath("source.step")));
  write(directory.filePath("other.blcad.json.sketch-constraints.json"), "Sidecar source");
  auto sidecar = checked(workbench.prepare_register(
      session, "Sidecar", "other.blcad.json.sketch-constraints.json", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, sidecar));
  CHECK_FALSE(session.save_as(directory.filePath("other.blcad.json").toStdString()));
  CHECK(bytes(directory.filePath("other.blcad.json.sketch-constraints.json")) == "Sidecar source");
}

TEST_CASE("Assembly definition import verifies embedded source roots and protects their assets",
          "[gui][step-import-workbench][step-import-source]") {
  GuiDocumentSession part;
  create(part, false);
  QTemporaryDir original, target;
  REQUIRE(part.save_as(original.filePath("part.blcad.json").toStdString()));
  register_source(part, original);
  GuiDocumentSession project;
  create(project, true);
  const GuiAssemblyWorkbench assembly;
  CHECK_FALSE(assembly.add_part(project, *part.part_document()));
  REQUIRE(project.save_as(target.filePath("model.blcad.project.json").toStdString()));
  CHECK_FALSE(assembly.add_part(project, *part.part_document()));
  write(target.filePath("source.step"), bytes(original.filePath("source.step")));
  REQUIRE(assembly.add_part(project, *part.part_document()));
  const auto inspected = checked(GuiStepImportWorkbench{}.inspect_sources(project));
  REQUIRE(inspected.size() == 1);
  CHECK(inspected.front().owner_document == part.part_document()->id());
  CHECK(inspected.front().embedded_part);
  CHECK_FALSE(
      validate_step_import_output_path(project, target.filePath("source.step").toStdString()));
  const auto browser = GuiDocumentBrowser::build(project);
  const auto& id = inspected.front().report.source().id().value();
  REQUIRE(browser.find("step-source/part/11:part.source/" + std::to_string(id.size()) + ":" + id));
}

TEST_CASE("STEP source presentation distinguishes registry kinds and delimited persistent ids",
          "[gui][step-import-workbench][step-import-source]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  auto project =
      checked(Project::create(DocumentId("a"), "Root project",
                              checked(AssemblyDocument::create(DocumentId("root"), "Root"))));
  const auto add_source = [&](auto& owner, const char* id, const char* path, const char* name) {
    write(directory.filePath(path), name);
    REQUIRE(owner.add_step_import_source(checked(
        StepImportSource::create(StepImportSourceId(id), name, path,
                                 checked(step_import_sha256(name)), StepImportMode::Reference))));
  };
  add_source(project, "b/c", "root.step", "Project source");
  auto same_id_part = checked(PartDocument::create(DocumentId("a"), "Same-id Part"));
  add_source(same_id_part, "b/c", "part.step", "Part source");
  REQUIRE(project.add_part_document(std::move(same_id_part)));
  auto slashed_part = checked(PartDocument::create(DocumentId("a/b"), "Slashed Part"));
  add_source(slashed_part, "c", "slashed.step", "Slashed source");
  REQUIRE(project.add_part_document(std::move(slashed_part)));
  const auto serialized = checked(serialize_project_to_json(project));
  const auto path = directory.filePath("model.blcad.project.json");
  write(path, QByteArray::fromStdString(serialized));
  GuiDocumentSession session;
  REQUIRE(session.open_file(path.toStdString()));
  const auto before = snapshot(session);
  const auto statuses = checked(GuiStepImportWorkbench{}.inspect_sources(session));
  REQUIRE(statuses.size() == 3);
  CHECK(statuses[0].owner_document == DocumentId("a"));
  CHECK_FALSE(statuses[0].embedded_part);
  CHECK(statuses[1].owner_document == DocumentId("a"));
  CHECK(statuses[1].embedded_part);
  CHECK(statuses[2].owner_document == DocumentId("a/b"));
  CHECK(statuses[2].embedded_part);
  for (const auto& status : statuses)
    CHECK(status.report.state() == StepImportFreshness::Available);
  const auto browser = GuiDocumentBrowser::build(session);
  const auto check_node = [&](const char* semantic_id, const char* owner, const char* source_id,
                              const char* name, const char* asset_path) {
    const auto* node = browser.find(semantic_id);
    REQUIRE(node);
    CHECK(node->kind == GuiBrowserNodeKind::ExternalSource);
    CHECK(node->owner_document_id == owner);
    CHECK(node->label == name);
    CHECK_FALSE(node->selection_kind);
    CHECK(node->viewport_semantic_id.empty());
    std::size_t matched = 0;
    for (const auto& property : node->properties) {
      CHECK_FALSE(property.editable);
      if (property.key == "id") {
        CHECK(property.value == source_id);
        ++matched;
      }
      if (property.key == "path") {
        CHECK(property.value == asset_path);
        ++matched;
      }
    }
    CHECK(matched == 2);
  };
  check_node("step-source/project/1:a/3:b/c", "a", "b/c", "Project source", "root.step");
  check_node("step-source/part/1:a/3:b/c", "a", "b/c", "Part source", "part.step");
  check_node("step-source/part/3:a/b/1:c", "a/b", "c", "Slashed source", "slashed.step");
  CHECK(snapshot(session) == before);
  CHECK(before == serialized);
  CHECK_FALSE(session.dirty());
  CHECK_FALSE(session.can_undo());
}

TEST_CASE(
    "Reference Part preparation applies shared headless candidates with atomic Project history",
    "[gui][step-reference-parts][block140-reference-parts]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  GuiDocumentSession session;
  create(session, true);
  REQUIRE(session.save_as(directory.filePath(filename(true)).toStdString()));
  write_geometry(directory.filePath("supplier.step"));
  const GuiStepImportWorkbench workbench;
  auto registration = checked(
      workbench.prepare_register(session, "Supplier", "supplier.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registration));
  auto topology = checked(workbench.prepare_topology(session, registration.source.id(), 0));
  REQUIRE(workbench.apply_topology(session, topology));
  const auto captured = snapshot(session);
  const auto history = session.undo_label();
  GuiStepReferenceRequest request;
  request.catalog = topology.catalog.id();
  request.new_part = DocumentId("part.reference");
  request.name = "Supplier Part";
  request.target_assembly = session.project()->assembly().id();
  request.instance = ComponentInstanceId("occurrence.first");
  auto preview = checked(workbench.prepare_reference(session, request));
  REQUIRE(preview.preview_shapes.size() == 1);
  CHECK(snapshot(session) == captured);
  CHECK(session.undo_label() == history);
  auto forged = preview;
  auto& forged_project =
      std::get<geometry::StepReferenceProjectCandidate>(forged.candidate).project;
  REQUIRE(forged_project.add_part_document(
      checked(PartDocument::create(DocumentId("part.injected"), "Injected"))));
  CHECK_FALSE(workbench.apply_reference(session, forged));
  CHECK(snapshot(session) == captured);
  REQUIRE(workbench.apply_reference(session, preview));
  const auto inserted = snapshot(session);
  REQUIRE(session.project()->part_document_count() == 1);
  const auto* reference = session.project()->find_part_document(request.new_part);
  REQUIRE(reference);
  CHECK(reference->is_reference_part());
  const auto binding = *reference->reference_part_definition();
  REQUIRE(binding.outputs().size() == 1);
  CHECK_FALSE(reference->bodies().empty());
  CHECK_FALSE(session.commit_project_transaction("Replace immutable Part", [&](Project& project) {
    *project.find_part_document(request.new_part) =
        checked(PartDocument::create(request.new_part, "Illicit replacement"));
    return Result<std::size_t>::success(1);
  }));
  CHECK(snapshot(session) == inserted);
  CHECK_FALSE(
      session.commit_project_transaction("Remove referenced immutable Part", [&](Project& project) {
        std::erase_if(project.part_documents(),
                      [&](const auto& part) { return part.id() == request.new_part; });
        return Result<std::size_t>::success(1);
      }));
  CHECK(snapshot(session) == inserted);
  const auto browser = GuiDocumentBrowser::build(session);
  const auto* body_node = browser.find(binding.outputs().front().body.value());
  REQUIRE(body_node);
  for (const auto& property : body_node->properties)
    CHECK_FALSE(property.editable);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == captured);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == inserted);
  CHECK_FALSE(workbench.apply_reference(session, preview));
  request.operation = GuiStepReferenceOperation::Repeat;
  request.reference_part = request.new_part;
  request.instance = ComponentInstanceId("occurrence.second");
  request.transform.translation_mm = {40, 0, 0};
  auto repeat = checked(workbench.prepare_reference(session, request));
  REQUIRE(workbench.apply_reference(session, repeat));
  CHECK(session.project()->part_document_count() == 1);
  REQUIRE(session.project()->assembly().component_instance_count() == 2);
  const auto repeated = snapshot(session);
  request.operation = GuiStepReferenceOperation::Convert;
  request.new_part = DocumentId("part.editable");
  request.name = "Editable Copy";
  auto convert = checked(workbench.prepare_reference(session, request));
  CHECK(snapshot(session) == repeated);
  REQUIRE(workbench.apply_reference(session, convert));
  REQUIRE(session.project()->part_document_count() == 2);
  const auto* editable = session.project()->find_part_document(request.new_part);
  REQUIRE(editable);
  CHECK_FALSE(editable->is_reference_part());
  CHECK(editable->imported_body_features().size() == 1);
  CHECK_FALSE(session.project()->assembly().has_member_part(editable->id()));
  for (const auto& occurrence : session.project()->assembly().component_instances())
    CHECK(occurrence.referenced_part_document() == request.reference_part);
  const auto converted = snapshot(session);
  REQUIRE(session.undo());
  CHECK(snapshot(session) == repeated);
  REQUIRE(session.redo());
  CHECK(snapshot(session) == converted);
  REQUIRE(session.save());
  REQUIRE(session.open_file(directory.filePath(filename(true)).toStdString()));
  CHECK(snapshot(session) == converted);
  const auto unused_id = DocumentId("part.unused.reference");
  auto unused = checked(geometry::StepReferenceCommands{}.create_definition(
      registration.source, topology.catalog, unused_id, "Unused Reference",
      directory.path().toStdString()));
  REQUIRE(session.commit_project_transaction(
      "Add unused Reference", [&](Project& project) { return project.add_part_document(unused); }));
  REQUIRE(session.commit_project_transaction("Remove unused Reference", [&](Project& project) {
    std::erase_if(project.part_documents(),
                  [&](const auto& part) { return part.id() == unused_id; });
    return Result<std::size_t>::success(1);
  }));
  CHECK_FALSE(session.project()->find_part_document(unused_id));
  CHECK(snapshot(session) == converted);
}

TEST_CASE("Standalone Reference creation and conversion save new protected copies without "
          "replacing intent",
          "[gui][step-reference-parts][block140-reference-parts]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  GuiDocumentSession session;
  create(session, false);
  const auto original_path = directory.filePath("source.blcad.json");
  REQUIRE(session.save_as(original_path.toStdString()));
  const auto asset_path = directory.filePath("supplier.step");
  write_geometry(asset_path);
  const auto asset_bytes = bytes(asset_path);
  const GuiStepImportWorkbench workbench;
  auto registration = checked(
      workbench.prepare_register(session, "Supplier", "supplier.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registration));
  auto topology = checked(workbench.prepare_topology(session, registration.source.id(), 0));
  REQUIRE(workbench.apply_topology(session, topology));
  REQUIRE(session.save());
  const auto original = snapshot(session);
  const auto original_file = bytes(original_path);
  GuiStepReferenceRequest request;
  request.catalog = topology.catalog.id();
  request.new_part = DocumentId("part.reference");
  request.name = "Reference Copy";
  auto create_reference = checked(workbench.prepare_reference(session, request));
  CHECK_FALSE(workbench.apply_reference(session, create_reference));
  const auto reference_path = directory.filePath("reference.blcad.json");
  REQUIRE(workbench.save_editable_copy(session, create_reference, reference_path.toStdString()));
  CHECK_FALSE(
      workbench.save_editable_copy(session, create_reference, reference_path.toStdString()));
  CHECK_FALSE(workbench.save_editable_copy(session, create_reference, original_path.toStdString()));
  CHECK_FALSE(workbench.save_editable_copy(session, create_reference, asset_path.toStdString()));
  CHECK(snapshot(session) == original);
  CHECK(bytes(original_path) == original_file);
  CHECK_FALSE(session.dirty());
  REQUIRE(session.open_file(reference_path.toStdString()));
  REQUIRE(session.part_document()->is_reference_part());
  REQUIRE(session.part_shape_cache());
  REQUIRE(session.part_shape_cache()->validated_reference_result(*session.part_document()));
  const auto reference = snapshot(session);
  CHECK_FALSE(session.commit_part_transaction("Replace immutable Part", [&](PartDocument& part) {
    part = checked(PartDocument::create(part.id(), "Illicit editable replacement"));
    return Result<std::size_t>::success(1);
  }));
  CHECK(snapshot(session) == reference);
  request.operation = GuiStepReferenceOperation::Convert;
  request.new_part = DocumentId("part.editable");
  request.name = "Editable Copy";
  auto conversion = checked(workbench.prepare_reference(session, request));
  const auto editable_path = directory.filePath("editable.blcad.json");
  REQUIRE(workbench.save_editable_copy(session, conversion, editable_path.toStdString()));
  CHECK(snapshot(session) == reference);
  CHECK_FALSE(session.can_undo());
  CHECK_FALSE(session.dirty());
  CHECK(bytes(asset_path) == asset_bytes);
  REQUIRE(session.open_file(editable_path.toStdString()));
  CHECK_FALSE(session.part_document()->is_reference_part());
  REQUIRE(session.part_document()->imported_body_features().size() == 1);
  REQUIRE(session.part_shape_cache());
  CHECK(session.part_shape_cache()->feature_shape_count() == 1);
  CHECK(session.part_shape_cache()->body_shape_count() == 1);
  CHECK_FALSE(session.part_shape_cache()->reference_result());
}

TEST_CASE(
    "Shared Reference refresh binds preview bytes and every owner before one history mutation",
    "[gui][step-reference-parts][block140-reference-parts]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  GuiDocumentSession session;
  create(session, true);
  REQUIRE(session.save_as(directory.filePath(filename(true)).toStdString()));
  const auto asset = directory.filePath("supplier.step");
  write_geometry(asset);
  const GuiStepImportWorkbench workbench;
  auto registration = checked(
      workbench.prepare_register(session, "Supplier", "supplier.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registration));
  auto topology = checked(workbench.prepare_topology(session, registration.source.id(), 0));
  REQUIRE(workbench.apply_topology(session, topology));
  GuiStepReferenceRequest request;
  request.catalog = topology.catalog.id();
  request.new_part = DocumentId("part.reference");
  request.name = "Supplier Part";
  request.target_assembly = session.project()->assembly().id();
  request.instance = ComponentInstanceId("occurrence.first");
  REQUIRE(
      workbench.apply_reference(session, checked(workbench.prepare_reference(session, request))));
  REQUIRE(session.save());
  const auto before = snapshot(session);
  request.operation = GuiStepReferenceOperation::Refresh;
  request.reference_part = request.new_part;
  auto current = checked(workbench.prepare_reference(session, request));
  REQUIRE(workbench.apply_reference(session, current));
  CHECK_FALSE(session.dirty());
  const auto revise = [&](const QByteArray& comment) {
    auto contents = bytes(asset);
    contents.replace(contents.indexOf("ENDSEC;"), 0, comment);
    write(asset, contents);
  };
  revise("/* Supplier revision B */\n");
  CHECK_FALSE(workbench.validate_reference(session, current));
  auto refresh = checked(workbench.prepare_reference(session, request));
  CHECK(snapshot(session) == before);
  CHECK(
      std::get<geometry::StepReferenceProjectCandidate>(refresh.candidate).affected_owners.size() ==
      2);
  revise("/* Supplier revision C */\n");
  CHECK_FALSE(workbench.apply_reference(session, refresh));
  CHECK(snapshot(session) == before);
  refresh = checked(workbench.prepare_reference(session, request));
  REQUIRE(workbench.apply_reference(session, refresh));
  const auto after = snapshot(session);
  CHECK(after != before);
  const auto* reference = session.project()->find_part_document(request.reference_part);
  REQUIRE(reference);
  CHECK(*reference->find_imported_topology_catalog(topology.catalog.id()) == topology.catalog);
  CHECK(reference->find_step_import_source(registration.source.id())->sha256_digest() ==
        session.project()->find_step_import_source(registration.source.id())->sha256_digest());
  REQUIRE(session.undo());
  CHECK(snapshot(session) == before);
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK_FALSE(session.reference_repair_mode());
  CHECK(session.can_redo());
  REQUIRE(session.redo());
  CHECK(snapshot(session) == after);
  CHECK(session.derived_results_fresh());
  write_geometry(asset, 60);
  CHECK_FALSE(workbench.prepare_reference(session, request));
  CHECK(snapshot(session) == after);
  REQUIRE(session.save());
  REQUIRE(session.open_file(directory.filePath(filename(true)).toStdString()));
  CHECK(snapshot(session) == after);
  CHECK_FALSE(session.dirty());
  CHECK(session.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK_FALSE(session.reference_repair_mode());
}

TEST_CASE("Project open and history retain accepted Reference intent when an active imported face "
          "is lost",
          "[gui][step-reference-parts][block140-reference-parts]") {
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto asset = directory.filePath("supplier.step");
  const auto project_path = directory.filePath("lost-face.blcad.project.json");
  const auto write_depth = [&](double depth) {
    auto shape = checked(geometry::RectangleExtrusionAdapter{}.make_extruded_rectangle(
        checked(Quantity::length_mm(20)), checked(Quantity::length_mm(10)),
        checked(Quantity::length_mm(depth))));
    REQUIRE(geometry::StepExporter{}.write_step(shape, asset.toStdString()));
  };
  write_depth(5);
  GuiDocumentSession session;
  create(session, true);
  REQUIRE(session.save_as(project_path.toStdString()));
  const GuiStepImportWorkbench workbench;
  const auto registration = checked(
      workbench.prepare_register(session, "Supplier", "supplier.step", StepImportMode::Reference));
  REQUIRE(workbench.apply(session, registration));
  const auto topology = checked(workbench.prepare_topology(session, registration.source.id(), 0));
  REQUIRE(workbench.apply_topology(session, topology));
  GuiStepReferenceRequest request;
  request.catalog = topology.catalog.id();
  request.new_part = DocumentId("part.reference");
  request.name = "Reference";
  request.target_assembly = session.project()->assembly().id();
  request.instance = ComponentInstanceId("first");
  REQUIRE(
      workbench.apply_reference(session, checked(workbench.prepare_reference(session, request))));
  request.operation = GuiStepReferenceOperation::Repeat;
  request.reference_part = request.new_part;
  request.instance = ComponentInstanceId("second");
  REQUIRE(
      workbench.apply_reference(session, checked(workbench.prepare_reference(session, request))));
  const ImportedTopologyEntry* top{};
  for (const auto& entry : topology.catalog.entries())
    if (entry.descriptor.kind == ImportedTopologyKind::Face &&
        entry.descriptor.geometry == ImportedGeometryClass::Plane &&
        std::abs(entry.descriptor.centroid.z - 5) < 1e-7)
      top = &entry;
  REQUIRE(top);
  const auto reference = checked(ImportedTopologyReference::create(
      topology.catalog.id(), top->id, ImportedTopologyCapability::Plane));
  const auto spelling = checked(make_imported_topology_target_spelling(reference));
  REQUIRE(session.commit_project_transaction("Use imported top face", [&](Project& project) {
    return project.assembly().add_constraint(checked(AssemblyConstraint::create(
        AssemblyConstraintId("mate"), "Imported Mate", AssemblyConstraintType::Mate,
        checked(AssemblyConstraintTarget::create(ComponentInstanceId("first"), spelling)),
        checked(AssemblyConstraintTarget::create(ComponentInstanceId("second"), spelling)))));
  }));
  // Core accepts source intent independently of Geometry. The new depth keeps
  // a complete bottom face, so the Body recovers while the selected top is Lost.
  auto accepted = *session.project();
  write_depth(8);
  auto observed =
      checked(inspect_step_import_asset(directory.path().toStdString(), "supplier.step"));
  REQUIRE(observed.observed_sha256_digest());
  auto* owned = accepted.find_part_document(request.reference_part);
  REQUIRE(owned);
  const auto expected = *owned->find_step_import_source(registration.source.id());
  REQUIRE(owned->refresh_step_import_source(expected, *observed.observed_sha256_digest()));
  REQUIRE(
      accepted.refresh_step_import_source(registration.source, *observed.observed_sha256_digest()));
  const auto accepted_bytes = bytes(asset);
  geometry::StepImportTopologyContext context(directory.path().toStdString());
  REQUIRE(geometry::ReferencePartGeometry{}.resolve(*owned, context));
  const auto recovery = checked(context.recover(*owned, topology.catalog.id()));
  REQUIRE(recovery->find(top->id));
  CHECK(recovery->find(top->id)->status == ImportedTopologyRecoveryStatus::Lost);
  const auto lost_json = checked(serialize_project_to_json(accepted));
  REQUIRE(write_project_json_file(accepted, project_path.toStdString()));
  GuiDocumentSession loaded;
  REQUIRE(loaded.open_file(project_path.toStdString()));
  CHECK(snapshot(loaded) == lost_json);
  CHECK(loaded.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK_FALSE(loaded.reference_repair_mode());
  CHECK_FALSE(loaded.dirty());
  CHECK_FALSE(loaded.can_undo());
  REQUIRE_FALSE(loaded.diagnostics().empty());
  CHECK(loaded.diagnostics().back().message.find("lost") != std::string::npos);
  CHECK_FALSE(loaded.commit_project_transaction(
      "Keep unresolved relationship", [](Project&) { return Result<std::size_t>::success(1); }));
  CHECK(snapshot(loaded) == lost_json);
  REQUIRE(
      loaded.commit_project_transaction("Remove unavailable relationship", [](Project& project) {
        auto replacement =
            checked(AssemblyDocument::create(project.assembly().id(), project.assembly().name()));
        for (const auto& member : project.assembly().member_parts())
          REQUIRE(replacement.add_member_part(member));
        for (const auto& component : project.assembly().component_instances())
          REQUIRE(replacement.add_component_instance(component));
        project.assembly() = std::move(replacement);
        return Result<std::size_t>::success(1);
      }));
  const auto repaired = snapshot(loaded);
  CHECK(loaded.derived_results_fresh());
  REQUIRE(loaded.undo());
  CHECK(snapshot(loaded) == lost_json);
  CHECK(loaded.recompute_status() == GuiRecomputeStatus::Failed);
  CHECK(loaded.can_redo());
  REQUIRE(loaded.redo());
  CHECK(snapshot(loaded) == repaired);
  CHECK(loaded.derived_results_fresh());
  auto malformed = nlohmann::json::parse(lost_json);
  // A genuinely Lost first endpoint must not hide a malformed second endpoint.
  malformed["assembly"]["assembly_constraints"][0]["target_b"]["semantic_reference"] =
      "imported:malformed";
  const auto malformed_path = directory.filePath("malformed.blcad.project.json");
  write(malformed_path, QByteArray::fromStdString(malformed.dump()));
  CHECK_FALSE(loaded.open_file(malformed_path.toStdString(), true));
  CHECK(snapshot(loaded) == repaired);
  CHECK(bytes(asset) == accepted_bytes);
}
